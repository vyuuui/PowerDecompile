#include "ir/GekkoTranslator.hh"

#include <algorithm>

#include "ir/RegisterBinding.hh"
#include "ppc/RegisterLiveness.hh"
#include "ppc/SubroutineGraph.hh"
#include "ppc/SubroutineStack.hh"
#include "utl/VariantOverloaded.hh"

namespace decomp::ir {
namespace {
IrType datatype_to_reftype(ppc::DataType dt) {
  switch (dt) {
    case ppc::DataType::kS1:
      return IrType::kS1;

    case ppc::DataType::kS2:
      return IrType::kS2;

    case ppc::DataType::kS4:
      return IrType::kS4;

    case ppc::DataType::kSingle:
      return IrType::kSingle;

    case ppc::DataType::kDouble:
      return IrType::kDouble;

    case ppc::DataType::kPackedSingle:
    case ppc::DataType::kSingleOrDouble:
    case ppc::DataType::kUnknown:
    default:
      return IrType::kInvalid;
  }
}

class GekkoTranslator {
  IrGraph _graph;
  IrBlock* _active_blk;

  ppc::Subroutine const& _routine;

private:
  OpVar translate_op(ppc::ReadSource op, uint32_t va) const;
  OpVar translate_op(ppc::WriteSource op, uint32_t va) const;
  OpVar translate_carry_op(uint32_t va) const;
  OpVar translate_ctr_op(uint32_t va) const;
  OpVar translate_lr_op(uint32_t va) const;

  // If the OE or RC flags are set, add the translation for them
  void translate_oerc(ppc::MetaInst const& inst, OpVar dest_op);

  IrInst& translate_dss_wrr(IrOpcode type, ppc::MetaInst const& inst);

  void translate_addi(IrOpcode type, ppc::MetaInst const& inst);
  void translate_addis(ppc::MetaInst const& inst);
  void translate_adde(ppc::MetaInst const& inst);
  void translate_adde_imm(ppc::MetaInst const& inst, OpVar imm);
  void translate_load(ppc::MetaInst const& inst);
  void translate_store(ppc::MetaInst const& inst);
  void translate_or(ppc::MetaInst const& inst);
  void translate_branch(ppc::MetaInst const& inst);
  void translate_branch_ctr(ppc::MetaInst const& inst);
  void translate_branch_lr(ppc::MetaInst const& inst);
  void translate_cmp(ppc::MetaInst const& inst, bool issigned);

  void translate_ppc_inst(ppc::MetaInst const& inst);
  void translate_block_transfer(ppc::BasicBlock const* from, ppc::BasicBlock const* to, ppc::OutgoingEdgeType etype);
  void translate_conditional_transfer(ppc::BasicBlock const* to, ppc::MetaInst const& inst, bool invert);
  void compute_block_binds(ppc::BasicBlock const& block);

public:
  GekkoTranslator(ppc::Subroutine const& routine) : _graph(routine), _routine(routine) {}

  void translate();
  IrGraph&& move_graph() { return std::move(_graph); }
};

void GekkoTranslator::compute_block_binds(ppc::BasicBlock const& block) {
  ppc::GprLiveness const* lt = block._gpr_lifetimes.get();
  ppc::GprSet connect_in = lt->_input;

  std::array<uint32_t, 32> rgn_begin;
  std::fill(rgn_begin.begin(), rgn_begin.end(), block._block_start);
  for (size_t i = 0; i < lt->_live_in.size(); i++) {
    const uint32_t cur_addr = block._block_start + 4 * i;
    ppc::GprSet delta_reg = lt->_live_in[i] ^ lt->_live_out[i];
    while (delta_reg) {
      ppc::GPR ref_gpr = static_cast<ppc::GPR>(__builtin_ctz(delta_reg._set));
      delta_reg -= ref_gpr;
      if (ppc::kAbiRegs.in_set(ref_gpr)) {
        continue;
      }
      if (lt->_live_in[i].in_set(ref_gpr)) {
        const uint32_t new_temp =
          _graph._gpr_binds.add_block_bind(ref_gpr, rgn_begin[static_cast<uint8_t>(ref_gpr)], cur_addr + 4);
        if (connect_in.in_set(ref_gpr)) {
          _graph._gpr_binds.publish_in(block, new_temp);
          connect_in -= ref_gpr;
        }
      } else {
        rgn_begin[static_cast<uint8_t>(ref_gpr)] = cur_addr;
      }
    }
  }

  ppc::GprSet out_set = lt->_output;
  while (out_set) {
    ppc::GPR out_gpr = static_cast<ppc::GPR>(__builtin_ctz(out_set._set));
    out_set -= out_gpr;
    if (ppc::kAbiRegs.in_set(out_gpr)) {
      continue;
    }

    const uint32_t new_temp =
      _graph._gpr_binds.add_block_bind(out_gpr, rgn_begin[static_cast<uint8_t>(out_gpr)], block._block_end);
    _graph._gpr_binds.publish_out(block, new_temp);
    if (connect_in.in_set(out_gpr)) {
      _graph._gpr_binds.publish_in(block, new_temp);
    }
  }
}

OpVar GekkoTranslator::translate_op(ppc::ReadSource op, uint32_t va) const {
  if (auto* gpr = std::get_if<ppc::GPRSlice>(&op); gpr != nullptr) {
    // TODO: signedness?
    GPRBindInfo const* gprb = _graph._gpr_binds.query_temp(va, gpr->_reg);
    return TVRef{TVTable::kGprTable, gprb->_num, datatype_to_reftype(gpr->_type)};
  } else if (auto* fpr = std::get_if<ppc::FPRSlice>(&op); fpr != nullptr) {
  } else if (auto* crb = std::get_if<ppc::CRBit>(&op); crb != nullptr) {
    return ConditionRef{static_cast<uint32_t>(*crb)};
  } else if (auto* mem = std::get_if<ppc::MemRegOff>(&op); mem != nullptr) {
    if (mem->_base == ppc::GPR::kR1) {
      if (_routine._stack->variable_for_offset(mem->_offset)->_is_param) {

      } else {
        return StackRef{mem->_offset};
      }
    } else if (mem->_base == ppc::GPR::kR2) {
      // TODO: read RTOC
    } else {
      GPRBindInfo const* gprb = _graph._gpr_binds.query_temp(va, mem->_base);
      return MemRef{gprb->_num, mem->_offset};
    }
  } else if (auto* mem = std::get_if<ppc::MemRegReg>(&op); mem != nullptr) {
  } else if (auto* spr = std::get_if<ppc::SPR>(&op); spr != nullptr) {
  } else if (auto* tbr = std::get_if<ppc::TBR>(&op); tbr != nullptr) {
  } else if (auto* frb = std::get_if<ppc::FPSCRBit>(&op); frb != nullptr) {
  } else if (auto* imm = std::get_if<ppc::SIMM>(&op); imm != nullptr) {
    return Immediate{static_cast<uint32_t>(static_cast<int32_t>(imm->_imm_value)), true};
  } else if (auto* imm = std::get_if<ppc::UIMM>(&op); imm != nullptr) {
    return Immediate{static_cast<uint32_t>(imm->_imm_value), false};
  } else if (auto* off = std::get_if<ppc::RelBranch>(&op); off != nullptr) {
  } else if (auto* imm = std::get_if<ppc::AuxImm>(&op); imm != nullptr) {
  }
  assert(false);
}

OpVar GekkoTranslator::translate_op(ppc::WriteSource op, uint32_t va) const {
  if (auto* gpr = std::get_if<ppc::GPRSlice>(&op); gpr != nullptr) {
    GPRBindInfo const* gprb = _graph._gpr_binds.query_temp(va, gpr->_reg);
    return TVRef{TVTable::kGprTable, gprb->_num, datatype_to_reftype(gpr->_type)};
  } else if (auto* fpr = std::get_if<ppc::FPRSlice>(&op); fpr != nullptr) {
  } else if (auto* crb = std::get_if<ppc::CRBit>(&op); crb != nullptr) {
    return ConditionRef{static_cast<uint32_t>(*crb)};
  } else if (auto* mem = std::get_if<ppc::MemRegOff>(&op); mem != nullptr) {
    if (mem->_base == ppc::GPR::kR1) {
      if (_routine._stack->variable_for_offset(mem->_offset)->_is_param) {

      } else {
        return StackRef{mem->_offset};
      }
    } else if (mem->_base == ppc::GPR::kR2) {
      // TODO: read RTOC
    } else {
      GPRBindInfo const* gprb = _graph._gpr_binds.query_temp(va, mem->_base);
      return MemRef{gprb->_num, mem->_offset};
    }
  } else if (auto* mem = std::get_if<ppc::MemRegReg>(&op); mem != nullptr) {
  } else if (auto* spr = std::get_if<ppc::SPR>(&op); spr != nullptr) {
  } else if (auto* tbr = std::get_if<ppc::TBR>(&op); tbr != nullptr) {
  } else if (auto* frb = std::get_if<ppc::FPSCRBit>(&op); frb != nullptr) {
  }
  assert(false);
}

OpVar GekkoTranslator::translate_carry_op(uint32_t va) const { assert(false); }

OpVar GekkoTranslator::translate_ctr_op(uint32_t va) const { assert(false); }

OpVar GekkoTranslator::translate_lr_op(uint32_t va) const { assert(false); }

void GekkoTranslator::translate_oerc(ppc::MetaInst const& inst, OpVar dest_op) {
  if (check_flags(inst._flags, ppc::InstFlags::kWritesRecord)) {
    _active_blk->_insts.emplace_back(IrOpcode::kRcTest, dest_op);
  }
  // TODO: other flags
}

IrInst& GekkoTranslator::translate_dss_wrr(IrOpcode type, ppc::MetaInst const& inst) {
  OpVar dest = translate_op(inst._writes[0], inst._va);
  OpVar src1 = translate_op(inst._reads[0], inst._va);
  OpVar src2 = translate_op(inst._reads[1], inst._va);

  return _active_blk->_insts.emplace_back(type, dest, src1, src2);
}

void GekkoTranslator::translate_addi(IrOpcode type, ppc::MetaInst const& inst) {
  if (std::holds_alternative<ppc::AuxImm>(inst._reads[0])) {
    OpVar src = translate_op(inst._reads[1], inst._va);
    OpVar dst = translate_op(inst._writes[0], inst._va);
    _active_blk->_insts.emplace_back(IrOpcode::kMov, dst, src);
  } else if (std::get<ppc::GPRSlice>(inst._reads[0])._reg == ppc::GPR::kR1) {
    _routine._stack->variable_for_offset(std::get<ppc::SIMM>(inst._reads[1])._imm_value);
  } else {
    translate_oerc(inst, translate_dss_wrr(type, inst)._ops[0]);
  }
}

void GekkoTranslator::translate_addis(ppc::MetaInst const& inst) {
  if (std::holds_alternative<ppc::AuxImm>(inst._reads[0])) {
    OpVar src = translate_op(inst._reads[1], inst._va);
    OpVar dst = translate_op(inst._writes[0], inst._va);
    std::get<Immediate>(dst)._val <<= 16;
    _active_blk->_insts.emplace_back(IrOpcode::kMov, dst, src);
  } else {
    IrInst& inst_tr = translate_dss_wrr(IrOpcode::kAdd, inst);
    std::get<Immediate>(inst_tr._ops[2])._val <<= 16;
    translate_oerc(inst, inst_tr._ops[0]);
  }
}

void GekkoTranslator::translate_adde(ppc::MetaInst const& inst) {
  IrInst& inst_tr = translate_dss_wrr(IrOpcode::kAdd, inst);
  _active_blk->_insts.emplace_back(IrOpcode::kAddc, inst_tr._ops[0], translate_carry_op(inst._va));
  translate_oerc(inst, inst_tr._ops[0]);
}

void GekkoTranslator::translate_adde_imm(ppc::MetaInst const& inst, OpVar imm) {
  OpVar dst = translate_op(inst._writes[0], inst._va);
  OpVar src = translate_op(inst._reads[0], inst._va);
  _active_blk->_insts.emplace_back(IrOpcode::kAddc, dst, src, imm);
  translate_oerc(inst, dst);
}

void GekkoTranslator::translate_load(ppc::MetaInst const& inst) {
  OpVar dst = translate_op(inst._writes[0], inst._va);
  OpVar src = translate_op(inst._reads[0], inst._va);
  _active_blk->_insts.emplace_back(IrOpcode::kLoad, dst, src);
}

void GekkoTranslator::translate_store(ppc::MetaInst const& inst) {
  OpVar dst = translate_op(inst._writes[0], inst._va);
  OpVar src = translate_op(inst._reads[0], inst._va);
  _active_blk->_insts.emplace_back(IrOpcode::kStore, dst, src);
}

void GekkoTranslator::translate_or(ppc::MetaInst const& inst) {
  if (inst._reads[0] == inst._reads[1]) {
    OpVar dst = translate_op(inst._writes[0], inst._va);
    OpVar src = translate_op(inst._reads[0], inst._va);
    _active_blk->_insts.emplace_back(IrOpcode::kMov, dst, src);
    translate_oerc(inst, dst);
  } else {
    translate_oerc(inst, translate_dss_wrr(IrOpcode::kOrB, inst)._ops[0]);
  }
}

void GekkoTranslator::translate_branch(ppc::MetaInst const& inst) {
  if (check_flags(inst._flags, ppc::InstFlags::kWritesLR)) {
    _active_blk->_insts.emplace_back(IrOpcode::kCall, FunctionRef{inst.branch_target()});
  }
  // Block transitions handled elsewhere
}

void GekkoTranslator::translate_branch_ctr(ppc::MetaInst const& inst) {
  if (check_flags(inst._flags, ppc::InstFlags::kWritesLR)) {
    _active_blk->_insts.emplace_back(IrOpcode::kCall, translate_ctr_op(inst._va));
  }
}

void GekkoTranslator::translate_branch_lr(ppc::MetaInst const& inst) {
  if (inst.is_blr()) {
    _active_blk->_insts.emplace_back(IrOpcode::kReturn);
  } else if (check_flags(inst._flags, ppc::InstFlags::kWritesLR)) {
    _active_blk->_insts.emplace_back(IrOpcode::kCall, translate_lr_op(inst._va));
  }
}

void GekkoTranslator::translate_cmp(ppc::MetaInst const& inst, bool issigned) {
  assert(!check_flags(inst._flags, ppc::InstFlags::kLongMode));

  translate_dss_wrr(IrOpcode::kCmp, inst);
  // mark as signed
  // ir_inst._ops[1]
}

void GekkoTranslator::translate_ppc_inst(ppc::MetaInst const& inst) {
  switch (inst._op) {
    case ppc::InstOperation::kAdd:
      translate_oerc(inst, translate_dss_wrr(IrOpcode::kAdd, inst)._ops[0]);
      break;

    case ppc::InstOperation::kAddc:
      translate_oerc(inst, translate_dss_wrr(IrOpcode::kAddc, inst)._ops[0]);
      break;

    case ppc::InstOperation::kAddi:
      translate_addi(IrOpcode::kAdd, inst);
      break;

    case ppc::InstOperation::kAddis:
      translate_addis(inst);
      break;

    case ppc::InstOperation::kAddic:
    case ppc::InstOperation::kAddicDot:
      translate_addi(IrOpcode::kAddc, inst);
      break;

    case ppc::InstOperation::kAdde:
      translate_adde(inst);
      break;

    case ppc::InstOperation::kAddme:
      translate_adde_imm(inst, Immediate{1, true});
      break;

    case ppc::InstOperation::kAddze:
      translate_adde_imm(inst, Immediate{0, false});
      break;

    case ppc::InstOperation::kDivw:
    case ppc::InstOperation::kDivwu:
    case ppc::InstOperation::kMulhw:
    case ppc::InstOperation::kMulhwu:
    case ppc::InstOperation::kMulli:
    case ppc::InstOperation::kMullw:
    case ppc::InstOperation::kNeg:
    case ppc::InstOperation::kSubf:
    case ppc::InstOperation::kSubfc:
    case ppc::InstOperation::kSubfe:
    case ppc::InstOperation::kSubfic:
    case ppc::InstOperation::kSubfme:
    case ppc::InstOperation::kSubfze:
      break;

    case ppc::InstOperation::kCmp:
    case ppc::InstOperation::kCmpi:
      translate_cmp(inst, true);
      break;

    case ppc::InstOperation::kCmpl:
    case ppc::InstOperation::kCmpli:
      translate_cmp(inst, false);
      break;

    case ppc::InstOperation::kAnd:
    case ppc::InstOperation::kAndc:
    case ppc::InstOperation::kAndiDot:
    case ppc::InstOperation::kAndisDot:
    case ppc::InstOperation::kCntlzw:
    case ppc::InstOperation::kEqv:
    case ppc::InstOperation::kExtsb:
    case ppc::InstOperation::kExtsh:
    case ppc::InstOperation::kNand:
    case ppc::InstOperation::kNor:
      break;

    case ppc::InstOperation::kOr:
      translate_or(inst);
      break;

    case ppc::InstOperation::kOrc:
    case ppc::InstOperation::kOri:
    case ppc::InstOperation::kOris:
    case ppc::InstOperation::kXor:
    case ppc::InstOperation::kXori:
    case ppc::InstOperation::kXoris:
    case ppc::InstOperation::kRlwimi:
    case ppc::InstOperation::kRlwinm:
    case ppc::InstOperation::kRlwnm:
    case ppc::InstOperation::kSlw:
    case ppc::InstOperation::kSraw:
    case ppc::InstOperation::kSrawi:
    case ppc::InstOperation::kSrw:
    case ppc::InstOperation::kFadd:
    case ppc::InstOperation::kFadds:
    case ppc::InstOperation::kFdiv:
    case ppc::InstOperation::kFdivs:
    case ppc::InstOperation::kFmul:
    case ppc::InstOperation::kFmuls:
    case ppc::InstOperation::kFres:
    case ppc::InstOperation::kFrsqrte:
    case ppc::InstOperation::kFsub:
    case ppc::InstOperation::kFsubs:
    case ppc::InstOperation::kFsel:
    case ppc::InstOperation::kFmadd:
    case ppc::InstOperation::kFmadds:
    case ppc::InstOperation::kFmsub:
    case ppc::InstOperation::kFmsubs:
    case ppc::InstOperation::kFnmadd:
    case ppc::InstOperation::kFnmadds:
    case ppc::InstOperation::kFnmsub:
    case ppc::InstOperation::kFnmsubs:
    case ppc::InstOperation::kFctiw:
    case ppc::InstOperation::kFctiwz:
    case ppc::InstOperation::kFrsp:
    case ppc::InstOperation::kFcmpo:
    case ppc::InstOperation::kFcmpu:
    case ppc::InstOperation::kMcrfs:
    case ppc::InstOperation::kMffs:
    case ppc::InstOperation::kMtfsb0:
    case ppc::InstOperation::kMtfsb1:
    case ppc::InstOperation::kMtfsf:
    case ppc::InstOperation::kMtfsfi:
    case ppc::InstOperation::kLbz:
    case ppc::InstOperation::kLbzu:
    case ppc::InstOperation::kLbzux:
    case ppc::InstOperation::kLbzx:
    case ppc::InstOperation::kLha:
    case ppc::InstOperation::kLhau:
    case ppc::InstOperation::kLhaux:
    case ppc::InstOperation::kLhax:
    case ppc::InstOperation::kLhz:
    case ppc::InstOperation::kLhzu:
    case ppc::InstOperation::kLhzux:
    case ppc::InstOperation::kLhzx:
      break;

    case ppc::InstOperation::kLwz:
      translate_load(inst);
      break;

    case ppc::InstOperation::kLwzu:
    case ppc::InstOperation::kLwzux:
    case ppc::InstOperation::kLwzx:
    case ppc::InstOperation::kStb:
    case ppc::InstOperation::kStbu:
    case ppc::InstOperation::kStbux:
    case ppc::InstOperation::kStbx:
    case ppc::InstOperation::kSth:
    case ppc::InstOperation::kSthu:
    case ppc::InstOperation::kSthux:
    case ppc::InstOperation::kSthx:
      break;

    case ppc::InstOperation::kStw:
      translate_store(inst);
      break;

    case ppc::InstOperation::kStwu:
    case ppc::InstOperation::kStwux:
    case ppc::InstOperation::kStwx:
    case ppc::InstOperation::kLhbrx:
    case ppc::InstOperation::kLwbrx:
    case ppc::InstOperation::kSthbrx:
    case ppc::InstOperation::kStwbrx:
    case ppc::InstOperation::kLmw:
    case ppc::InstOperation::kStmw:
    case ppc::InstOperation::kLswi:
    case ppc::InstOperation::kLswx:
    case ppc::InstOperation::kStswi:
    case ppc::InstOperation::kStswx:
    case ppc::InstOperation::kEieio:
    case ppc::InstOperation::kIsync:
    case ppc::InstOperation::kLwarx:
    case ppc::InstOperation::kStwcxDot:
    case ppc::InstOperation::kSync:
    case ppc::InstOperation::kLfd:
    case ppc::InstOperation::kLfdu:
    case ppc::InstOperation::kLfdux:
    case ppc::InstOperation::kLfdx:
    case ppc::InstOperation::kLfs:
    case ppc::InstOperation::kLfsu:
    case ppc::InstOperation::kLfsux:
    case ppc::InstOperation::kLfsx:
    case ppc::InstOperation::kStfd:
    case ppc::InstOperation::kStfdu:
    case ppc::InstOperation::kStfdux:
    case ppc::InstOperation::kStfdx:
    case ppc::InstOperation::kStfiwx:
    case ppc::InstOperation::kStfs:
    case ppc::InstOperation::kStfsu:
    case ppc::InstOperation::kStfsux:
    case ppc::InstOperation::kStfsx:
    case ppc::InstOperation::kFabs:
    case ppc::InstOperation::kFmr:
    case ppc::InstOperation::kFnabs:
    case ppc::InstOperation::kFneg:
      break;

    case ppc::InstOperation::kB:
      translate_branch(inst);
      break;

    case ppc::InstOperation::kBc:
      break;

    case ppc::InstOperation::kBcctr:
      translate_branch_ctr(inst);
      break;

    case ppc::InstOperation::kBclr:
      translate_branch_lr(inst);
      break;

    case ppc::InstOperation::kCrand:
    case ppc::InstOperation::kCrandc:
    case ppc::InstOperation::kCreqv:
    case ppc::InstOperation::kCrnand:
    case ppc::InstOperation::kCrnor:
    case ppc::InstOperation::kCror:
    case ppc::InstOperation::kCrorc:
    case ppc::InstOperation::kCrxor:
    case ppc::InstOperation::kMcrf:
    case ppc::InstOperation::kRfi:
    case ppc::InstOperation::kSc:
    case ppc::InstOperation::kTw:
    case ppc::InstOperation::kTwi:
    case ppc::InstOperation::kMcrxr:
    case ppc::InstOperation::kMfcr:
    case ppc::InstOperation::kMfmsr:
    case ppc::InstOperation::kMfspr:
    case ppc::InstOperation::kMftb:
    case ppc::InstOperation::kMtcrf:
    case ppc::InstOperation::kMtmsr:
    case ppc::InstOperation::kMtspr:
    case ppc::InstOperation::kDcbf:
    case ppc::InstOperation::kDcbi:
    case ppc::InstOperation::kDcbst:
    case ppc::InstOperation::kDcbt:
    case ppc::InstOperation::kDcbtst:
    case ppc::InstOperation::kDcbz:
    case ppc::InstOperation::kIcbi:
    case ppc::InstOperation::kMfsr:
    case ppc::InstOperation::kMfsrin:
    case ppc::InstOperation::kMtsr:
    case ppc::InstOperation::kMtsrin:
    case ppc::InstOperation::kTlbie:
    case ppc::InstOperation::kTlbsync:
    case ppc::InstOperation::kEciwx:
    case ppc::InstOperation::kEcowx:
    case ppc::InstOperation::kPsq_lx:
    case ppc::InstOperation::kPsq_stx:
    case ppc::InstOperation::kPsq_lux:
    case ppc::InstOperation::kPsq_stux:
    case ppc::InstOperation::kPsq_l:
    case ppc::InstOperation::kPsq_lu:
    case ppc::InstOperation::kPsq_st:
    case ppc::InstOperation::kPsq_stu:
    case ppc::InstOperation::kPs_div:
    case ppc::InstOperation::kPs_sub:
    case ppc::InstOperation::kPs_add:
    case ppc::InstOperation::kPs_sel:
    case ppc::InstOperation::kPs_res:
    case ppc::InstOperation::kPs_mul:
    case ppc::InstOperation::kPs_rsqrte:
    case ppc::InstOperation::kPs_msub:
    case ppc::InstOperation::kPs_madd:
    case ppc::InstOperation::kPs_nmsub:
    case ppc::InstOperation::kPs_nmadd:
    case ppc::InstOperation::kPs_neg:
    case ppc::InstOperation::kPs_mr:
    case ppc::InstOperation::kPs_nabs:
    case ppc::InstOperation::kPs_abs:
    case ppc::InstOperation::kPs_sum0:
    case ppc::InstOperation::kPs_sum1:
    case ppc::InstOperation::kPs_muls0:
    case ppc::InstOperation::kPs_muls1:
    case ppc::InstOperation::kPs_madds0:
    case ppc::InstOperation::kPs_madds1:
    case ppc::InstOperation::kPs_cmpu0:
    case ppc::InstOperation::kPs_cmpo0:
    case ppc::InstOperation::kPs_cmpu1:
    case ppc::InstOperation::kPs_cmpo1:
    case ppc::InstOperation::kPs_merge00:
    case ppc::InstOperation::kPs_merge01:
    case ppc::InstOperation::kPs_merge10:
    case ppc::InstOperation::kPs_merge11:
    case ppc::InstOperation::kDcbz_l:
    case ppc::InstOperation::kInvalid:
      break;
  }
}

void GekkoTranslator::translate_block_transfer(
  ppc::BasicBlock const* from, ppc::BasicBlock const* to, ppc::OutgoingEdgeType etype) {
  switch (etype) {
    case ppc::OutgoingEdgeType::kUnconditional:
    case ppc::OutgoingEdgeType::kFallthrough:
      _active_blk->_tr_out.emplace_back(to->_block_id, 0, 0, true);
      break;

    case ppc::OutgoingEdgeType::kConditionTrue:
      translate_conditional_transfer(to, from->_instructions.back(), true);
      break;

    case ppc::OutgoingEdgeType::kConditionFalse:
      translate_conditional_transfer(to, from->_instructions.back(), false);
      break;
  }
}

void GekkoTranslator::translate_conditional_transfer(ppc::BasicBlock const* to, ppc::MetaInst const& inst, bool taken) {
  if (inst._op == ppc::InstOperation::kBc) {
    const uint32_t crb = static_cast<uint32_t>(std::get<ppc::CRBit>(inst._reads[0]));

    switch (ppc::bo_type_from_imm(inst._binst.bo())) {
      case ppc::BOType::kT:
        _active_blk->_tr_out.emplace_back(to->_block_id, crb, 0, taken);
        break;

      case ppc::BOType::kF:
        _active_blk->_tr_out.emplace_back(to->_block_id, crb, crb, taken);
        break;

      case ppc::BOType::kAlways:
        if (taken) {
          _active_blk->_tr_out.emplace_back(to->_block_id, 0, 0, true);
        }
        break;

      case ppc::BOType::kDz:
        _active_blk->_tr_out.emplace_back(to->_block_id, 0, 0, taken, CounterCheck::kCounterZero);
        break;

      case ppc::BOType::kDnz:
        _active_blk->_tr_out.emplace_back(to->_block_id, 0, 0, taken, CounterCheck::kCounterNotZero);
        break;

      case ppc::BOType::kDzt:
        _active_blk->_tr_out.emplace_back(to->_block_id, crb, 0, taken, CounterCheck::kCounterZero);
        break;

      case ppc::BOType::kDzf:
        _active_blk->_tr_out.emplace_back(to->_block_id, crb, crb, taken, CounterCheck::kCounterZero);
        break;

      case ppc::BOType::kDnzt:
        _active_blk->_tr_out.emplace_back(to->_block_id, crb, 0, taken, CounterCheck::kCounterNotZero);
        break;

      case ppc::BOType::kDnzf:
        _active_blk->_tr_out.emplace_back(to->_block_id, crb, crb, taken, CounterCheck::kCounterNotZero);
        break;

      default:
        assert(false);
        break;
    }
  } else if (inst._op == ppc::InstOperation::kBc) {
    // TODO: conditional returns
    assert(false);
  } else {
    assert(false);
  }
}

void GekkoTranslator::translate() {
  ppc::dfs_forward(
    [this](ppc::BasicBlock const* cur) { compute_block_binds(*cur); }, ppc::always_iterate, _routine._graph->_root);
  _graph._gpr_binds.collect_block_scope_temps();

  ppc::dfs_forward(
    [this](ppc::BasicBlock const* cur) {
      _active_blk = &_graph._blocks[cur->_block_id];
      for (size_t i = 0; i < cur->_instructions.size(); i++) {
        if (cur->_perilogue_types[i] == ppc::PerilogueInstructionType::kNormalInst) {
          translate_ppc_inst(cur->_instructions[i]);
        }
      }
      for (auto const& [etype, dest] : cur->_outgoing_edges) {
        translate_block_transfer(cur, dest, etype);
      }
    },
    ppc::always_iterate,
    _routine._graph->_root);
}
}  // namespace

IrGraph translate_subroutine(ppc::Subroutine const& routine) {
  GekkoTranslator tr(routine);
  tr.translate();

  return tr.move_graph();
}
}  // namespace decomp::ir
