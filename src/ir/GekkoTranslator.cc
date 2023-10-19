#include "ir/GekkoTranslator.hh"

#include <algorithm>

#include "ir/RegisterBinding.hh"
#include "ppc/RegisterLiveness.hh"
#include "ppc/SubroutineGraph.hh"
#include "utl/VariantOverloaded.hh"

namespace decomp::ir {
namespace {
class GekkoTranslator {
  IrGraph _graph;
  IrBlock* _active_blk;

  ppc::Subroutine const& _routine;

private:
  [[maybe_unused]] OpVar translate_op(ppc::ReadSource op) const;
  [[maybe_unused]] OpVar translate_op(ppc::WriteSource op) const;

  void translate_ppc_inst(ppc::MetaInst const& inst);
  void translate_add(ppc::MetaInst const& inst);
  void compute_binds_local(ppc::BasicBlock const& block);

public:
  GekkoTranslator(ppc::Subroutine const& routine) : _graph(routine), _routine(routine) {}

  void translate();
  IrGraph&& move_graph() { return std::move(_graph); }
};

constexpr ppc::GprSet kAbiRegs = ppc::gpr_mask_literal<ppc::GPR::kR1, ppc::GPR::kR2, ppc::GPR::kR13>();

void GekkoTranslator::compute_binds_local(ppc::BasicBlock const& block) {
  auto const* lt = block._block_lifetimes;
  ppc::GprSet connect_in = lt->_input;

  std::array<uint32_t, 32> rgn_begin;
  std::fill(rgn_begin.begin(), rgn_begin.end(), block._block_start);
  for (size_t i = 0; i < lt->_live_in.size(); i++) {
    const uint32_t cur_addr = block._block_start + 4 * i;
    ppc::GprSet delta_reg = lt->_live_in[i] ^ lt->_live_out[i];
    while (delta_reg) {
      ppc::GPR ref_gpr = static_cast<ppc::GPR>(__builtin_ctz(delta_reg._set));
      delta_reg -= ref_gpr;
      if (kAbiRegs.in_set(ref_gpr)) {
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
    if (kAbiRegs.in_set(out_gpr)) {
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

OpVar GekkoTranslator::translate_op(ppc::ReadSource op) const {
  if (auto* gpr = std::get_if<ppc::GPRSlice>(&op); gpr != nullptr) {
  } else if (auto* fpr = std::get_if<ppc::FPRSlice>(&op); fpr != nullptr) {
  } else if (auto* crb = std::get_if<ppc::CRBit>(&op); crb != nullptr) {
  } else if (auto* mem = std::get_if<ppc::MemRegOff>(&op); mem != nullptr) {
  } else if (auto* mem = std::get_if<ppc::MemRegReg>(&op); mem != nullptr) {
  } else if (auto* spr = std::get_if<ppc::SPR>(&op); spr != nullptr) {
  } else if (auto* tbr = std::get_if<ppc::TBR>(&op); tbr != nullptr) {
  } else if (auto* frb = std::get_if<ppc::FPSCRBit>(&op); frb != nullptr) {
  } else if (auto* imm = std::get_if<ppc::SIMM>(&op); imm != nullptr) {
  } else if (auto* imm = std::get_if<ppc::UIMM>(&op); imm != nullptr) {
  } else if (auto* off = std::get_if<ppc::RelBranch>(&op); off != nullptr) {
  } else if (auto* imm = std::get_if<ppc::AuxImm>(&op); imm != nullptr) {
  }
  assert(false);
}

OpVar GekkoTranslator::translate_op(ppc::WriteSource op) const {
  if (auto* gpr = std::get_if<ppc::GPRSlice>(&op); gpr != nullptr) {
  } else if (auto* fpr = std::get_if<ppc::FPRSlice>(&op); fpr != nullptr) {
  } else if (auto* crb = std::get_if<ppc::CRBit>(&op); crb != nullptr) {
  } else if (auto* mem = std::get_if<ppc::MemRegOff>(&op); mem != nullptr) {
  } else if (auto* mem = std::get_if<ppc::MemRegReg>(&op); mem != nullptr) {
  } else if (auto* spr = std::get_if<ppc::SPR>(&op); spr != nullptr) {
  } else if (auto* tbr = std::get_if<ppc::TBR>(&op); tbr != nullptr) {
  } else if (auto* frb = std::get_if<ppc::FPSCRBit>(&op); frb != nullptr) {
  }
  assert(false);
}

void GekkoTranslator::translate_ppc_inst(ppc::MetaInst const& inst) {
  switch (inst._op) {
    case ppc::InstOperation::kAdd:
    case ppc::InstOperation::kAddc:
    case ppc::InstOperation::kAdde:
    case ppc::InstOperation::kAddi:
    case ppc::InstOperation::kAddic:
    case ppc::InstOperation::kAddicDot:
    case ppc::InstOperation::kAddis:
    case ppc::InstOperation::kAddme:
    case ppc::InstOperation::kAddze:
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
    case ppc::InstOperation::kCmp:
    case ppc::InstOperation::kCmpi:
    case ppc::InstOperation::kCmpl:
    case ppc::InstOperation::kCmpli:
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
    case ppc::InstOperation::kOr:
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
    case ppc::InstOperation::kLwz:
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
    case ppc::InstOperation::kStw:
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
    case ppc::InstOperation::kB:
    case ppc::InstOperation::kBc:
    case ppc::InstOperation::kBcctr:
    case ppc::InstOperation::kBclr:
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

void GekkoTranslator::translate() {
  ppc::dfs_forward(
    [this](ppc::BasicBlock const* cur) { compute_binds_local(*cur); }, ppc::always_iterate, _routine._graph._root);
  _graph._gpr_binds.collect_local_temps();

  ppc::dfs_forward(
    [this](ppc::BasicBlock const* cur) {
      _active_blk = &_graph._blocks.emplace_back();
      for (ppc::MetaInst const& inst : cur->_instructions) {
        translate_ppc_inst(inst);
      }
    },
    ppc::always_iterate,
    _routine._graph._root);
}
}  // namespace

IrGraph translate_subroutine(ppc::Subroutine const& routine) {
  GekkoTranslator tr(routine);
  tr.translate();

  return tr.move_graph();
}
}  // namespace decomp::ir
