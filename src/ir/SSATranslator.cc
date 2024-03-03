#include "ir/SSATranslator.hh"

namespace decomp::ir {
namespace {
using namespace ppc;

void translate_ssa_block(BasicBlockVertex const& ppc, SSABlockVertex& ssa) {
  BasicBlock const& bb = ppc.data();
  SSABlock& ssabb = ssa.data();

  // Add a write-read-read inst
  const auto wrr = [&ssabb](MetaInst const& inst, SSAOpcode opc, DataSize sz, SignednessHint hint = SignednessHint::kNoHint) {
    ssabb._insts.emplace_back(opc, SSAOperand(inst._writes[0], sz), SSAOperand(inst._reads[0], sz),
                              SSAOperand(inst._reads[1], sz), hint);
  };
  const auto oerc = [&ssabb](MetaInst const& inst, DataSize sz) {
    if (check_flags(inst._flags, InstFlags::kWritesRecord)) {
      ssabb._insts.emplace_back(SSAOpcode::kCmp, SSAOperand(inst._writes[0], sz), SSAOperand(Immediate{0, true}, sz),
                                SignednessHint::kSigned);
    }
    if (check_flags(inst._flags, InstFlags::kWritesXER)) {
      // TODO: track overflow
    }
  };

  for (auto inst : bb._instructions) {
    switch (inst._op) {
        case InstOperation::kAdd:
        case InstOperation::kAddi: {
          wrr(inst, SSAOpcode::kAdd, DataSize::kS32);
          oerc(inst, DataSize::kS32);
          break;
        }

        case InstOperation::kAddc:
        case InstOperation::kAddic:
        case InstOperation::kAddicDot: {
          wrr(inst, SSAOpcode::kAdd, DataSize::kS32);
          // TODO: track carry
          oerc(inst, DataSize::kS32);
          break;
        }

        case InstOperation::kAdde:
          wrr(inst, SSAOpcode::kAdd, DataSize::kS32);
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[2], DataSize::kS32));
          // TODO: track carry
          oerc(inst, DataSize::kS32);
          break;

        // Need to shift the immediate left 16 bits
        case InstOperation::kAddis: {
          AuxImm shifted = {std::get<SIMM>(inst._reads[1])._imm_value << 16};
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    SSAOperand(ReadSource(shifted), DataSize::kS32));
          break;
        }

        case InstOperation::kAddme: {
          SIMM negative_one = {-1};
          wrr(inst, SSAOpcode::kAdd, DataSize::kS32);
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(negative_one, DataSize::kS32));
          // TODO: track carry
          oerc(inst, DataSize::kS32);
          break;
        }

        case InstOperation::kAddze:
          wrr(inst, SSAOpcode::kAdd, DataSize::kS32);
          // TODO: track carry
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kDivw:
          wrr(inst, SSAOpcode::kDiv, DataSize::kS32, SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kDivwu:
          wrr(inst, SSAOpcode::kDiv, DataSize::kS32, SignednessHint::kUnsigned);
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kMulhw:
          wrr(inst, SSAOpcode::kMul, DataSize::kS32, SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kMulhwu:
          wrr(inst, SSAOpcode::kMul, DataSize::kS32, SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kMulli:
          wrr(inst, SSAOpcode::kMul, DataSize::kS32, SignednessHint::kSigned);
          break;

        case InstOperation::kMullw:
          wrr(inst, SSAOpcode::kMul, DataSize::kS32, SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;
        
        case InstOperation::kNeg:
          ssabb._insts.emplace_back(SSAOpcode::kNeg,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kSubf:
          ssabb._insts.emplace_back(SSAOpcode::kSub,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[1], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kSubfc:
        case InstOperation::kSubfic:
          ssabb._insts.emplace_back(SSAOpcode::kSub,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[1], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    SignednessHint::kSigned);
          // TODO: track carry
          oerc(inst, DataSize::kS32);
          break;
          
        case InstOperation::kSubfe:
          ssabb._insts.emplace_back(SSAOpcode::kSub,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[1], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    SignednessHint::kSigned);
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[2], DataSize::kS32),
                                    SignednessHint::kSigned);
          // TODO: track carry
          oerc(inst, DataSize::kS32);
          break;
          
        case InstOperation::kSubfme:
          SIMM negative_one = {-1};
          ssabb._insts.emplace_back(SSAOpcode::kSub,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[1], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    SignednessHint::kSigned);
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(negative_one, DataSize::kS32),
                                    SignednessHint::kSigned);
          // TODO: track carry
          oerc(inst, DataSize::kS32);
          break;
        
        case InstOperation::kSubfze:
          ssabb._insts.emplace_back(SSAOpcode::kSub,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[1], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    SignednessHint::kSigned);
          //TODO: track carry
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kCmp:
        case InstOperation::kCmpi:
          wrr(inst, SSAOpcode::kCmp, DataSize::kS32, SignednessHint::kSigned);
          break;

        case InstOperation::kCmpl:
        case InstOperation::kCmpli:
          wrr(inst, SSAOpcode::kCmp, DataSize::kS32, SignednessHint::kUnsigned);
          break;

        case InstOperation::kAnd:
          wrr(inst, SSAOpcode::kAndB, DataSize::kS32);
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kAndc: {
          SSAOperand complement = SSAOperand::mktemp_int(DataSize::kS32);
          ssabb._insts.emplace_back(SSAOpcode::kNotB,
                                    complement,
                                    SSAOperand(inst._reads[1], DataSize::kS32),
                                    SignednessHint::kSigned);
          ssabb._insts.emplace_back(SSAOpcode::kAndB,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    complement,
                                    SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;
        }

        case InstOperation::kAndiDot: {
          ssabb._insts.emplace_back(SSAOpcode::kAndB,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;
        }
        case InstOperation::kAndisDot: {
          AuxImm shifted = {std::get<SIMM>(inst._reads[1])._imm_value << 16};
          ssabb._insts.emplace_back(SSAOpcode::kAndB,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    shifted,
                                    SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;
        }
        case InstOperation::kCntlzw:
        case InstOperation::kEqv:
        case InstOperation::kExtsb:
        case InstOperation::kExtsh:
        case InstOperation::kNand:
        case InstOperation::kNor:
        case InstOperation::kOr:
        case InstOperation::kOrc:
        case InstOperation::kOri:
        case InstOperation::kOris:
        case InstOperation::kXor:
        case InstOperation::kXori:
        case InstOperation::kXoris:
        case InstOperation::kRlwimi:
        case InstOperation::kRlwinm:
        case InstOperation::kRlwnm:
        case InstOperation::kSlw:
        case InstOperation::kSraw:
        case InstOperation::kSrawi:
        case InstOperation::kSrw:
          break;

        case InstOperation::kFadd:
          wrr(inst, SSAOpcode::kAdd, DataSize::kS64, SignednessHint::kSigned);
          // TODO: carry?
          oerc(inst, DataSize::kS64);
          break;
        case InstOperation::kFadds:
          wrr(inst, SSAOpcode::kAdd, DataSize::kS32, SignednessHint::kSigned);
          // TODO: carry?
          oerc(inst, DataSize::kS32);
          break;

        case InstOperation::kFdiv:
        case InstOperation::kFdivs:
        case InstOperation::kFmul:
          wrr(inst, SSAOpcode::kMul, DataSize::kS64, SignednessHint::kSigned);
          oerc(inst, DataSize::kS64);
          break;
        case InstOperation::kFmuls:
          wrr(inst, SSAOpcode::kMul, DataSize::kS32, SignednessHint::kSigned);
          oerc(inst, DataSize::kS32);
          break;
        case InstOperation::kFres:
        case InstOperation::kFrsqrte:
        case InstOperation::kFsub:
        case InstOperation::kFsubs:
        case InstOperation::kFsel:
        case InstOperation::kFmadd:
        case InstOperation::kFmadds:
        case InstOperation::kFmsub:
        case InstOperation::kFmsubs:
        case InstOperation::kFnmadd:
        case InstOperation::kFnmadds:
        case InstOperation::kFnmsub:
        case InstOperation::kFnmsubs:
        case InstOperation::kFctiw:
        case InstOperation::kFctiwz:
        case InstOperation::kFrsp:
        case InstOperation::kFcmpo:
        case InstOperation::kFcmpu:
          break;
          // stuff below to the break untranslatable
        case InstOperation::kMcrfs:
        case InstOperation::kMffs:
        case InstOperation::kMtfsb0:
        case InstOperation::kMtfsb1:
        case InstOperation::kMtfsf:
        case InstOperation::kMtfsfi:
          break;
        case InstOperation::kLbz:
        case InstOperation::kLbzu:
        case InstOperation::kLbzux:
        case InstOperation::kLbzx:
        // 
        case InstOperation::kLha:
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS16),
                                    SSAOperand(inst._reads[0], DataSize::kS16),
                                    SignednessHint::kSigned);
          break;
        case InstOperation::kLhz:
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS16),
                                    SSAOperand(inst._reads[0], DataSize::kS16),
                                    SignednessHint::kUnsigned);
          break;
        case InstOperation::kLhau: {
          Immediate d = {
            ._val = std::get<MemRegOff>(inst._reads[0])._offset,
            ._signed = true
          };
          
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(inst._writes[1], DataSize::kS32),
                                    SSAOperand(inst._writes[1], DataSize::kS32),
                                    SSAOperand(d, DataSize::kS32),
                                    SignednessHint::kUnsigned);
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS16),
                                    SSAOperand(inst._reads[0], DataSize::kS16),
                                    SignednessHint::kSigned);
          break;
        }
        case InstOperation::kLhzu: {
          Immediate d = {
            ._val = std::get<MemRegOff>(inst._reads[0])._offset,
            ._signed = true
          };
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(inst._writes[1], DataSize::kS32),
                                    SSAOperand(inst._writes[1], DataSize::kS32),
                                    SSAOperand(d, DataSize::kS32),
                                    SignednessHint::kUnsigned);
          ssabb._insts.emplace_back(SSAOpcode::kMov,
                                    SSAOperand(inst._writes[0], DataSize::kS16),
                                    SSAOperand(inst._reads[0], DataSize::kS16),
                                    SignednessHint::kUnsigned);
          break;
        }
        case InstOperation::kLhaux:
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS16),
                                    SSAOperand(inst._reads[0], DataSize::kS16),
                                    SignednessHint::kSigned);
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(ppc::WriteSource(inst._binst.ra_w()), DataSize::kS32),
                                    SSAOperand(ppc::ReadSource(inst._binst.ra_w()), DataSize::kS32),
                                    SSAOperand(ppc::ReadSource(inst._binst.rb_w()), DataSize::kS32),
                                    SignednessHint::kUnsigned);
          break;
        case InstOperation::kLhzux:
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS16),
                                    SSAOperand(inst._reads[0], DataSize::kS16),
                                    SignednessHint::kUnsigned);
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(ppc::WriteSource(inst._binst.ra_w()), DataSize::kS32),
                                    SSAOperand(ppc::ReadSource(inst._binst.ra_w()), DataSize::kS32),
                                    SSAOperand(ppc::ReadSource(inst._binst.rb_w()), DataSize::kS32),
                                    SignednessHint::kUnsigned);
          break;
        case InstOperation::kLhax:
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS16),
                                    SSAOperand(inst._reads[0], DataSize::kS16),
                                    SignednessHint::kSigned);
          break;
        case InstOperation::kLhzx:
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS16),
                                    SSAOperand(inst._reads[0], DataSize::kS16),
                                    SignednessHint::kUnsigned);
          break;
        case InstOperation::kLwz:
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32));
          break;
        case InstOperation::kLwzu: {
          Immediate d = {
            ._val = std::get<MemRegOff>(inst._reads[0])._offset,
            ._signed = true
          };
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(inst._writes[1], DataSize::kS32),
                                    SSAOperand(inst._writes[1], DataSize::kS32),
                                    SSAOperand(d, DataSize::kS32),
                                    SignednessHint::kUnsigned);
          ssabb._insts.emplace_back(SSAOpcode::kMov,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    SignednessHint::kUnsigned);
          break;
        }
        case InstOperation::kLwzux:
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32));
          ssabb._insts.emplace_back(SSAOpcode::kAdd,
                                    SSAOperand(ppc::WriteSource(inst._binst.ra_w()), DataSize::kS32),
                                    SSAOperand(ppc::ReadSource(inst._binst.ra_w()), DataSize::kS32),
                                    SSAOperand(ppc::ReadSource(inst._binst.rb_w()), DataSize::kS32),
                                    SignednessHint::kUnsigned);
          break;
        case InstOperation::kLwzx:
          ssabb._insts.emplace_back(SSAOpcode::kMov, 
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32));
          break;
        case InstOperation::kStb:
        case InstOperation::kStbu:
        case InstOperation::kStbux:
        case InstOperation::kStbx:
        case InstOperation::kSth:
        case InstOperation::kSthu:
        case InstOperation::kSthux:
        case InstOperation::kSthx:
        case InstOperation::kStw:
        case InstOperation::kStwu:
        case InstOperation::kStwux:
        case InstOperation::kStwx:
        case InstOperation::kLhbrx:
        case InstOperation::kLwbrx:
        case InstOperation::kSthbrx:
        case InstOperation::kStwbrx:
        case InstOperation::kLmw:
        case InstOperation::kStmw:
        case InstOperation::kLswi:
        case InstOperation::kLswx:
        case InstOperation::kStswi:
        case InstOperation::kStswx:
        case InstOperation::kEieio:
        case InstOperation::kIsync:
        case InstOperation::kLwarx:
        case InstOperation::kStwcxDot:
        case InstOperation::kSync:
        case InstOperation::kLfd:
        case InstOperation::kLfdu:
        case InstOperation::kLfdux:
        case InstOperation::kLfdx:
        case InstOperation::kLfs:
        case InstOperation::kLfsu:
        case InstOperation::kLfsux:
        case InstOperation::kLfsx:
        case InstOperation::kStfd:
        case InstOperation::kStfdu:
        case InstOperation::kStfdux:
        case InstOperation::kStfdx:
        case InstOperation::kStfiwx:
        case InstOperation::kStfs:
        case InstOperation::kStfsu:
        case InstOperation::kStfsux:
        case InstOperation::kStfsx:
        case InstOperation::kFabs:
          ssabb._insts.emplace_back(SSAOpcode::kAbs,
                                    SSAOperand(inst._writes[0], DataSize::kS64),
                                    SSAOperand(inst._reads[0], DataSize::kS64),
                                    SignednessHint::kSigned);
          oerc(inst, DataSize::kS64);
        case InstOperation::kFmr:
        case InstOperation::kFnabs:
        case InstOperation::kFneg:
          ssabb._insts.emplace_back(SSAOpcode::kNeg,
                                    SSAOperand(inst._writes[0], DataSize::kS64),
                                    SSAOperand(inst._reads[0], DataSize::kS64),
                                    SignednessHint::kSigned);
          oerc(inst, DataSize::kS64);
        case InstOperation::kB:
        case InstOperation::kBc:
        case InstOperation::kBcctr:
        case InstOperation::kBclr:
        case InstOperation::kCrand:
        case InstOperation::kCrandc:
        case InstOperation::kCreqv:
        case InstOperation::kCrnand:
        case InstOperation::kCrnor:
          break;
        //
        case InstOperation::kCror:
          wrr(inst, SSAOpcode::kOrB, DataSize::kS32);
          break;

        case InstOperation::kCrorc: {
          SSAOperand temp_bit = SSAOperand::mktemp_cond(DataSize::kS32);
          ssabb._insts.emplace_back(SSAOpcode::kNotB, temp_bit, SSAOperand(inst._reads[1], DataSize::kS32));
          ssabb._insts.emplace_back(SSAOpcode::kOrB,
                                    SSAOperand(inst._writes[0], DataSize::kS32),
                                    SSAOperand(inst._reads[0], DataSize::kS32),
                                    temp_bit);
          break;
        }

        case InstOperation::kCrxor:
        case InstOperation::kMcrf:
        case InstOperation::kRfi:
        case InstOperation::kSc:
        case InstOperation::kTw:
        case InstOperation::kTwi:
        case InstOperation::kMcrxr:
        case InstOperation::kMfcr:
        case InstOperation::kMfmsr:
        case InstOperation::kMfspr:
        case InstOperation::kMftb:
        case InstOperation::kMtcrf:
        case InstOperation::kMtmsr:
        case InstOperation::kMtspr:
        case InstOperation::kDcbf:
        case InstOperation::kDcbi:
        case InstOperation::kDcbst:
        case InstOperation::kDcbt:
        case InstOperation::kDcbtst:
        case InstOperation::kDcbz:
        case InstOperation::kIcbi:
        case InstOperation::kMfsr:
        case InstOperation::kMfsrin:
        case InstOperation::kMtsr:
        case InstOperation::kMtsrin:
        case InstOperation::kTlbie:
        case InstOperation::kTlbsync:
        case InstOperation::kEciwx:
        case InstOperation::kEcowx:
        case InstOperation::kPsq_lx:
        case InstOperation::kPsq_stx:
        case InstOperation::kPsq_lux:
        case InstOperation::kPsq_stux:
        case InstOperation::kPsq_l:
        case InstOperation::kPsq_lu:
        case InstOperation::kPsq_st:
        case InstOperation::kPsq_stu:
        case InstOperation::kPs_div:
        case InstOperation::kPs_sub:
        case InstOperation::kPs_add:
        case InstOperation::kPs_sel:
        case InstOperation::kPs_res:
        case InstOperation::kPs_mul:
        case InstOperation::kPs_rsqrte:
        case InstOperation::kPs_msub:
        case InstOperation::kPs_madd:
        case InstOperation::kPs_nmsub:
        case InstOperation::kPs_nmadd:
        case InstOperation::kPs_neg:
        case InstOperation::kPs_mr:
        case InstOperation::kPs_nabs:
        case InstOperation::kPs_abs:
        case InstOperation::kPs_sum0:
        case InstOperation::kPs_sum1:
        case InstOperation::kPs_muls0:
        case InstOperation::kPs_muls1:
        case InstOperation::kPs_madds0:
        case InstOperation::kPs_madds1:
        case InstOperation::kPs_cmpu0:
        case InstOperation::kPs_cmpo0:
        case InstOperation::kPs_cmpu1:
        case InstOperation::kPs_cmpo1:
        case InstOperation::kPs_merge00:
        case InstOperation::kPs_merge01:
        case InstOperation::kPs_merge10:
        case InstOperation::kPs_merge11:
        case InstOperation::kDcbz_l:
        case InstOperation::kInvalid:
    }
  }
}
}

SSARoutine translate_to_ssa(ppc::Subroutine const& routine) {
  // 1. Basic PPC->SSA (copy routine blocks, fill insts with SSA instead of PPC),
  //    do not fill operands
  // 2. Determine the SSA index for each operand of each ppc inst, add operands to SSA insts
  // 3. Insert PHI functions for merged values
  SSARoutine ret;

  // step 1
  ret._graph.copy_shape_generator(routine._graph.get(), translate_ssa_block);


}
}