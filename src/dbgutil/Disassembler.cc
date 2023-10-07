#include "dbgutil/Disassembler.hh"

#include <fmt/format.h>

#include <ostream>

#include "ppc/PpcDisasm.hh"
#include "utl/VariantOverloaded.hh"

namespace decomp {
namespace {
std::string opcode_name(InstOperation op) {
  switch (op) {
    case InstOperation::kAdd:
      return "add";
    case InstOperation::kAddc:
      return "addc";
    case InstOperation::kAdde:
      return "adde";
    case InstOperation::kAddi:
      return "addi";
    case InstOperation::kAddic:
      return "addic";
    case InstOperation::kAddicDot:
      return "addic.";
    case InstOperation::kAddis:
      return "addis";
    case InstOperation::kAddme:
      return "addme";
    case InstOperation::kAddze:
      return "addze";
    case InstOperation::kDivw:
      return "divw";
    case InstOperation::kDivwu:
      return "divwu";
    case InstOperation::kMulhw:
      return "mulhw";
    case InstOperation::kMulhwu:
      return "mulhwu";
    case InstOperation::kMulli:
      return "mulli";
    case InstOperation::kMullw:
      return "mullw";
    case InstOperation::kNeg:
      return "neg";
    case InstOperation::kSubf:
      return "subf";
    case InstOperation::kSubfc:
      return "subfc";
    case InstOperation::kSubfe:
      return "subfe";
    case InstOperation::kSubfic:
      return "subfic";
    case InstOperation::kSubfme:
      return "subfme";
    case InstOperation::kSubfze:
      return "subfze";
    case InstOperation::kCmp:
      return "cmp";
    case InstOperation::kCmpi:
      return "cmpi";
    case InstOperation::kCmpl:
      return "cmpl";
    case InstOperation::kCmpli:
      return "cmpli";
    case InstOperation::kAnd:
      return "and";
    case InstOperation::kAndc:
      return "andc";
    case InstOperation::kAndiDot:
      return "andi.";
    case InstOperation::kAndisDot:
      return "andis.";
    case InstOperation::kCntlzw:
      return "cntlzw";
    case InstOperation::kEqv:
      return "eqv";
    case InstOperation::kExtsb:
      return "extsb";
    case InstOperation::kExtsh:
      return "extsh";
    case InstOperation::kNand:
      return "nand";
    case InstOperation::kNor:
      return "nor";
    case InstOperation::kOr:
      return "or";
    case InstOperation::kOrc:
      return "orc";
    case InstOperation::kOri:
      return "ori";
    case InstOperation::kOris:
      return "oris";
    case InstOperation::kXor:
      return "xor";
    case InstOperation::kXori:
      return "xori";
    case InstOperation::kXoris:
      return "xoris";
    case InstOperation::kRlwimi:
      return "rlwimi";
    case InstOperation::kRlwinm:
      return "rlwinm";
    case InstOperation::kRlwnm:
      return "rlwnm";
    case InstOperation::kSlw:
      return "slw";
    case InstOperation::kSraw:
      return "sraw";
    case InstOperation::kSrawi:
      return "srawi";
    case InstOperation::kSrw:
      return "srw";
    case InstOperation::kFadd:
      return "fadd";
    case InstOperation::kFadds:
      return "fadds";
    case InstOperation::kFdiv:
      return "fdiv";
    case InstOperation::kFdivs:
      return "fdivs";
    case InstOperation::kFmul:
      return "fmul";
    case InstOperation::kFmuls:
      return "fmuls";
    case InstOperation::kFres:
      return "fres";
    case InstOperation::kFrsqrte:
      return "frsqrte";
    case InstOperation::kFsub:
      return "fsub";
    case InstOperation::kFsubs:
      return "fsubs";
    case InstOperation::kFsel:
      return "fsel";
    case InstOperation::kFmadd:
      return "fmadd";
    case InstOperation::kFmadds:
      return "fmadds";
    case InstOperation::kFmsub:
      return "fmsub";
    case InstOperation::kFmsubs:
      return "fmsubs";
    case InstOperation::kFnmadd:
      return "fnmadd";
    case InstOperation::kFnmadds:
      return "fnmadds";
    case InstOperation::kFnmsub:
      return "fnmsub";
    case InstOperation::kFnmsubs:
      return "fnmsubs";
    case InstOperation::kFctiw:
      return "fctiw";
    case InstOperation::kFctiwz:
      return "fctiwz";
    case InstOperation::kFrsp:
      return "frsp";
    case InstOperation::kFcmpo:
      return "fcmpo";
    case InstOperation::kFcmpu:
      return "fcmpu";
    case InstOperation::kMcrfs:
      return "mcrfs";
    case InstOperation::kMffs:
      return "mffs";
    case InstOperation::kMtfsb0:
      return "mtfsb0";
    case InstOperation::kMtfsb1:
      return "mtfsb1";
    case InstOperation::kMtfsf:
      return "mtfsf";
    case InstOperation::kMtfsfi:
      return "mtfsfi";
    case InstOperation::kLbz:
      return "lbz";
    case InstOperation::kLbzu:
      return "lbzu";
    case InstOperation::kLbzux:
      return "lbzux";
    case InstOperation::kLbzx:
      return "lbzx";
    case InstOperation::kLha:
      return "lha";
    case InstOperation::kLhau:
      return "lhau";
    case InstOperation::kLhaux:
      return "lhaux";
    case InstOperation::kLhax:
      return "lhax";
    case InstOperation::kLhz:
      return "lhz";
    case InstOperation::kLhzu:
      return "lhzu";
    case InstOperation::kLhzux:
      return "lhzux";
    case InstOperation::kLhzx:
      return "lhzx";
    case InstOperation::kLwz:
      return "lwz";
    case InstOperation::kLwzu:
      return "lwzu";
    case InstOperation::kLwzux:
      return "lwzux";
    case InstOperation::kLwzx:
      return "lwzx";
    case InstOperation::kStb:
      return "stb";
    case InstOperation::kStbu:
      return "stbu";
    case InstOperation::kStbux:
      return "stbux";
    case InstOperation::kStbx:
      return "stbx";
    case InstOperation::kSth:
      return "sth";
    case InstOperation::kSthu:
      return "sthu";
    case InstOperation::kSthux:
      return "sthux";
    case InstOperation::kSthx:
      return "sthx";
    case InstOperation::kStw:
      return "stw";
    case InstOperation::kStwu:
      return "stwu";
    case InstOperation::kStwux:
      return "stwux";
    case InstOperation::kStwx:
      return "stwx";
    case InstOperation::kLhbrx:
      return "lhbrx";
    case InstOperation::kLwbrx:
      return "lwbrx";
    case InstOperation::kSthbrx:
      return "sthbrx";
    case InstOperation::kStwbrx:
      return "stwbrx";
    case InstOperation::kLmw:
      return "lmw";
    case InstOperation::kStmw:
      return "stmw";
    case InstOperation::kLswi:
      return "lswi";
    case InstOperation::kLswx:
      return "lswx";
    case InstOperation::kStswi:
      return "stswi";
    case InstOperation::kStswx:
      return "stswx";
    case InstOperation::kEieio:
      return "eieio";
    case InstOperation::kIsync:
      return "isync";
    case InstOperation::kLwarx:
      return "lwarx";
    case InstOperation::kStwcxDot:
      return "stwcx.";
    case InstOperation::kSync:
      return "sync";
    case InstOperation::kLfd:
      return "lfd";
    case InstOperation::kLfdu:
      return "lfdu";
    case InstOperation::kLfdux:
      return "lfdux";
    case InstOperation::kLfdx:
      return "lfdx";
    case InstOperation::kLfs:
      return "lfs";
    case InstOperation::kLfsu:
      return "lfsu";
    case InstOperation::kLfsux:
      return "lfsux";
    case InstOperation::kLfsx:
      return "lfsx";
    case InstOperation::kStfd:
      return "stfd";
    case InstOperation::kStfdu:
      return "stfdu";
    case InstOperation::kStfdux:
      return "stfdux";
    case InstOperation::kStfdx:
      return "stfdx";
    case InstOperation::kStfiwx:
      return "stfiwx";
    case InstOperation::kStfs:
      return "stfs";
    case InstOperation::kStfsu:
      return "stfsu";
    case InstOperation::kStfsux:
      return "stfsux";
    case InstOperation::kStfsx:
      return "stfsx";
    case InstOperation::kFabs:
      return "fabs";
    case InstOperation::kFmr:
      return "fmr";
    case InstOperation::kFnabs:
      return "fnabs";
    case InstOperation::kFneg:
      return "fneg";
    case InstOperation::kB:
      return "b";
    case InstOperation::kBc:
      return "bc";
    case InstOperation::kBcctr:
      return "bcctr";
    case InstOperation::kBclr:
      return "bclr";
    case InstOperation::kCrand:
      return "crand";
    case InstOperation::kCrandc:
      return "crandc";
    case InstOperation::kCreqv:
      return "creqv";
    case InstOperation::kCrnand:
      return "crnand";
    case InstOperation::kCrnor:
      return "crnor";
    case InstOperation::kCror:
      return "cror";
    case InstOperation::kCrorc:
      return "crorc";
    case InstOperation::kCrxor:
      return "crxor";
    case InstOperation::kMcrf:
      return "mcrf";
    case InstOperation::kRfi:
      return "rfi";
    case InstOperation::kSc:
      return "sc";
    case InstOperation::kTw:
      return "tw";
    case InstOperation::kTwi:
      return "twi";
    case InstOperation::kMcrxr:
      return "mcrxr";
    case InstOperation::kMfcr:
      return "mfcr";
    case InstOperation::kMfmsr:
      return "mfmsr";
    case InstOperation::kMfspr:
      return "mfspr";
    case InstOperation::kMftb:
      return "mftb";
    case InstOperation::kMtcrf:
      return "mtcrf";
    case InstOperation::kMtmsr:
      return "mtmsr";
    case InstOperation::kMtspr:
      return "mtspr";
    case InstOperation::kDcbf:
      return "dcbf";
    case InstOperation::kDcbi:
      return "dcbi";
    case InstOperation::kDcbst:
      return "dcbst";
    case InstOperation::kDcbt:
      return "dcbt";
    case InstOperation::kDcbtst:
      return "dcbtst";
    case InstOperation::kDcbz:
      return "dcbz";
    case InstOperation::kIcbi:
      return "icbi";
    case InstOperation::kMfsr:
      return "mfsr";
    case InstOperation::kMfsrin:
      return "mfsrin";
    case InstOperation::kMtsr:
      return "mtsr";
    case InstOperation::kMtsrin:
      return "mtsrin";
    case InstOperation::kTlbie:
      return "tlbie";
    case InstOperation::kTlbsync:
      return "tlbsync";
    case InstOperation::kEciwx:
      return "eciwx";
    case InstOperation::kEcowx:
      return "ecowx";
    case InstOperation::kPsq_lx:
      return "psq_lx";
    case InstOperation::kPsq_stx:
      return "psq_stx";
    case InstOperation::kPsq_lux:
      return "psq_lux";
    case InstOperation::kPsq_stux:
      return "psq_stux";
    case InstOperation::kPsq_l:
      return "psq_l";
    case InstOperation::kPsq_lu:
      return "psq_lu";
    case InstOperation::kPsq_st:
      return "psq_st";
    case InstOperation::kPsq_stu:
      return "psq_stu";
    case InstOperation::kPs_div:
      return "ps_div";
    case InstOperation::kPs_sub:
      return "ps_sub";
    case InstOperation::kPs_add:
      return "ps_add";
    case InstOperation::kPs_sel:
      return "ps_sel";
    case InstOperation::kPs_res:
      return "ps_res";
    case InstOperation::kPs_mul:
      return "ps_mul";
    case InstOperation::kPs_rsqrte:
      return "ps_rsqrte";
    case InstOperation::kPs_msub:
      return "ps_msub";
    case InstOperation::kPs_madd:
      return "ps_madd";
    case InstOperation::kPs_nmsub:
      return "ps_nmsub";
    case InstOperation::kPs_nmadd:
      return "ps_nmadd";
    case InstOperation::kPs_neg:
      return "ps_neg";
    case InstOperation::kPs_mr:
      return "ps_mr";
    case InstOperation::kPs_nabs:
      return "ps_nabs";
    case InstOperation::kPs_abs:
      return "ps_abs";
    case InstOperation::kPs_sum0:
      return "ps_sum0";
    case InstOperation::kPs_sum1:
      return "ps_sum1";
    case InstOperation::kPs_muls0:
      return "ps_muls0";
    case InstOperation::kPs_muls1:
      return "ps_muls1";
    case InstOperation::kPs_madds0:
      return "ps_madds0";
    case InstOperation::kPs_madds1:
      return "ps_madds1";
    case InstOperation::kPs_cmpu0:
      return "ps_cmpu0";
    case InstOperation::kPs_cmpo0:
      return "ps_cmpo0";
    case InstOperation::kPs_cmpu1:
      return "ps_cmpu1";
    case InstOperation::kPs_cmpo1:
      return "ps_cmpo1";
    case InstOperation::kPs_merge00:
      return "ps_merge00";
    case InstOperation::kPs_merge01:
      return "ps_merge01";
    case InstOperation::kPs_merge10:
      return "ps_merge10";
    case InstOperation::kPs_merge11:
      return "ps_merge11";
    case InstOperation::kDcbz_l:
      return "dcbz_l";
    default:
      return "invalid";
  }
}

char const* opcode_flags_string(InstOperation opcode, BinInst inst) {
  InstFlags live_oerc = inst.oe() | inst.rc();
  InstFlags live_lkaa = inst.lk() | inst.aa();
  switch (opcode) {
    case InstOperation::kAdd:
    case InstOperation::kAddc:
    case InstOperation::kAdde:
    case InstOperation::kAddme:
    case InstOperation::kAddze:
    case InstOperation::kDivw:
    case InstOperation::kDivwu:
    case InstOperation::kMullw:
    case InstOperation::kNeg:
    case InstOperation::kSubf:
    case InstOperation::kSubfc:
    case InstOperation::kSubfe:
    case InstOperation::kSubfme:
    case InstOperation::kSubfze:
    case InstOperation::kXor:
      if (live_oerc == (InstFlags::kWritesXER | InstFlags::kWritesRecord)) {
        return "o.";
      } else if (live_oerc == InstFlags::kWritesXER) {
        return "o";
      } else if (live_oerc == InstFlags::kWritesRecord) {
        return ".";
      } else {
        return "";
      }

    case InstOperation::kAnd:
    case InstOperation::kAndc:
    case InstOperation::kCntlzw:
    case InstOperation::kEqv:
    case InstOperation::kExtsb:
    case InstOperation::kExtsh:
    case InstOperation::kFabs:
    case InstOperation::kFadd:
    case InstOperation::kFadds:
    case InstOperation::kFctiw:
    case InstOperation::kFctiwz:
    case InstOperation::kFdiv:
    case InstOperation::kFdivs:
    case InstOperation::kFmadd:
    case InstOperation::kFmadds:
    case InstOperation::kFmr:
    case InstOperation::kFmsub:
    case InstOperation::kFmsubs:
    case InstOperation::kFmul:
    case InstOperation::kFmuls:
    case InstOperation::kFnabs:
    case InstOperation::kFneg:
    case InstOperation::kFnmadd:
    case InstOperation::kFnmadds:
    case InstOperation::kFnmsub:
    case InstOperation::kFnmsubs:
    case InstOperation::kFres:
    case InstOperation::kFrsp:
    case InstOperation::kFrsqrte:
    case InstOperation::kFsel:
    case InstOperation::kFsub:
    case InstOperation::kFsubs:
    case InstOperation::kMffs:
    case InstOperation::kMtfsb0:
    case InstOperation::kMtfsb1:
    case InstOperation::kMtfsf:
    case InstOperation::kMtfsfi:
    case InstOperation::kMulhw:
    case InstOperation::kMulhwu:
    case InstOperation::kNand:
    case InstOperation::kNor:
    case InstOperation::kOr:
    case InstOperation::kOrc:
    case InstOperation::kPs_abs:
    case InstOperation::kPs_add:
    case InstOperation::kPs_div:
    case InstOperation::kPs_madd:
    case InstOperation::kPs_madds0:
    case InstOperation::kPs_madds1:
    case InstOperation::kPs_merge00:
    case InstOperation::kPs_merge01:
    case InstOperation::kPs_merge10:
    case InstOperation::kPs_merge11:
    case InstOperation::kPs_mr:
    case InstOperation::kPs_msub:
    case InstOperation::kPs_mul:
    case InstOperation::kPs_muls0:
    case InstOperation::kPs_muls1:
    case InstOperation::kPs_nabs:
    case InstOperation::kPs_neg:
    case InstOperation::kPs_nmadd:
    case InstOperation::kPs_nmsub:
    case InstOperation::kPs_res:
    case InstOperation::kPs_rsqrte:
    case InstOperation::kPs_sel:
    case InstOperation::kPs_sub:
    case InstOperation::kPs_sum0:
    case InstOperation::kPs_sum1:
    case InstOperation::kRlwimi:
    case InstOperation::kRlwinm:
    case InstOperation::kRlwnm:
    case InstOperation::kSlw:
    case InstOperation::kSraw:
    case InstOperation::kSrawi:
    case InstOperation::kSrw:
      if (check_flags(live_oerc, InstFlags::kWritesRecord)) {
        return ".";
      } else {
        return "";
      }

    case InstOperation::kB:
    case InstOperation::kBc:
      if (live_lkaa == (InstFlags::kWritesLR | InstFlags::kAbsoluteAddr)) {
        return "la";
      } else if (live_lkaa == InstFlags::kWritesLR) {
        return "l";
      } else if (live_lkaa == InstFlags::kAbsoluteAddr) {
        return "a";
      } else {
        return "";
      }

    case InstOperation::kBcctr:
    case InstOperation::kBclr:
      if (check_flags(live_lkaa, InstFlags::kWritesLR)) {
        return "l";
      } else {
        return "";
      }

    default:
      return "";
  }
}

char const* condition_string(uint8_t cfb) {
  switch (cfb & 0b11) {
    case 0:
      return "lt";
    case 1:
      return "gt";
    case 2:
      return "eq";
    case 3:
      return "so";
    default:
      return "";
  }
}
char const* condition_string_inv(uint8_t cfb) {
  switch (cfb & 0b11) {
    case 0:
      return "ge";
    case 1:
      return "le";
    case 2:
      return "ne";
    case 3:
      return "ns";
    default:
      return "";
  }
}
std::string disasm_crbit(uint8_t crbit) {
  if (crbit >= 4) {
    return fmt::format("cr{}*4+{}", crbit / 4, condition_string(crbit & 0b11));
  }
  return condition_string(crbit & 0b11);
}
std::string disasm_reg(GPR reg) { return fmt::format("r{}", static_cast<int>(reg)); }
std::string disasm_reg(FPR reg) { return fmt::format("f{}", static_cast<int>(reg)); }

enum Field {
  _Spr,
  _Tbr,
  _Bd,
  _Bi,
  _Bo,
  _Crba,
  _Crbb,
  _Crbd,
  _Crfd,
  _Crfs,
  _Crm,
  _D16ra,
  _D20ra,
  _Fm,
  _Fra,
  _Frb,
  _Frc,
  _Frd,
  _Frs,
  _I17,
  _I22,
  _Imm,
  _L,
  _Li,
  _Mb,
  _Me,
  _Nb,
  _Ra,
  _Rb,
  _Rd,
  _Rs,
  _Sh,
  _Simm,
  _Sr,
  _To,
  _Uimm,
  _W
};

// Generic operand writer
template <Field f>
void write_operand(BinInst inst, std::ostream& sink) {
  if constexpr (f == _Spr) {
    switch (inst.spr()) {
      case SPR::kCtr:
        sink << "CTR";
        break;

      case SPR::kLr:
        sink << "LR";
        break;

      case SPR::kXer:
        sink << "XER";
        break;

      default:
        sink << static_cast<int>(inst.spr());
        break;
    }
  } else if constexpr (f == _Tbr) {
    switch (inst.tbr()) {
      case TBR::kTbl:
        sink << "TBL";
        break;

      case TBR::kTbu:
        sink << "TBU";
        break;

      default:
        sink << static_cast<int>(inst.tbr());
        break;
    }
  } else if constexpr (f == _Crba) {
    sink << disasm_crbit(inst.crba_val());
  } else if constexpr (f == _Crbb) {
    sink << disasm_crbit(inst.crbb_val());
  } else if constexpr (f == _Crbd) {
    sink << disasm_crbit(inst.crbd_val());
  } else if constexpr (f == _Crfd) {
    sink << fmt::format("cr{}", inst.crfd_val());
  } else if constexpr (f == _Crfs) {
    sink << fmt::format("cr{}", inst.crfs_val());
  } else if constexpr (f == _Crm) {
    sink << fmt::format("{:#x}", inst.crm_val());
  } else if constexpr (f == _D16ra) {
    sink << fmt::format("{:#x}({})", inst.d16(), disasm_reg(inst.ra()));
  } else if constexpr (f == _D20ra) {
    sink << fmt::format("{:#x}({})", inst.d20(), disasm_reg(inst.ra()));
  } else if constexpr (f == _Fm) {
    sink << fmt::format("{:#x}", inst.fm_val());
  } else if constexpr (f == _Fra) {
    sink << disasm_reg(inst.fra());
  } else if constexpr (f == _Frb) {
    sink << disasm_reg(inst.frb());
  } else if constexpr (f == _Frc) {
    sink << disasm_reg(inst.frc());
  } else if constexpr (f == _Frd) {
    sink << disasm_reg(inst.frd());
  } else if constexpr (f == _Frs) {
    sink << disasm_reg(inst.frs());
  } else if constexpr (f == _I17) {
    sink << inst.i17()._val;
  } else if constexpr (f == _I22) {
    sink << inst.i22()._val;
  } else if constexpr (f == _Imm) {
    sink << inst.imm()._val;
  } else if constexpr (f == _L) {
    sink << inst.l_val();
  } else if constexpr (f == _Mb) {
    sink << inst.mb()._val;
  } else if constexpr (f == _Me) {
    sink << inst.me()._val;
  } else if constexpr (f == _Nb) {
    sink << inst.nb()._val;
  } else if constexpr (f == _Ra) {
    sink << disasm_reg(inst.ra());
  } else if constexpr (f == _Rb) {
    sink << disasm_reg(inst.rb());
  } else if constexpr (f == _Rd) {
    sink << disasm_reg(inst.rd());
  } else if constexpr (f == _Rs) {
    sink << disasm_reg(inst.rs());
  } else if constexpr (f == _Sh) {
    sink << inst.sh()._val;
  } else if constexpr (f == _Simm) {
    int32_t val = inst.simm()._imm_value;
    if (abs(val) < 16) {
      sink << val;
    } else {
      sink << fmt::format("{:#x}", val);
    }
  } else if constexpr (f == _Sr) {
    sink << inst.sr()._val;
  } else if constexpr (f == _To) {
    sink << inst.to()._val;
  } else if constexpr (f == _Uimm) {
    uint32_t val = inst.uimm()._imm_value;
    if (val < 16) {
      sink << val;
    } else {
      sink << fmt::format("{:#x}", val);
    }
  } else if constexpr (f == _W) {
    sink << inst.w_val();
  }
}

// Generic operand list writer
template <Field f0, Field... fields>
void write_inst_operands(const MetaInst& inst, std::ostream& sink) {
  write_operand<f0>(inst._binst, sink);
  if constexpr (sizeof...(fields) > 0) {
    sink << ", ";
    write_inst_operands<fields...>(inst, sink);
  }
}

void write_rel_loc(MetaInst const& inst, std::ostream& sink) {
  sink << fmt::format("{:#x} // -> loc_{:08x}", std::get<RelBranch>(inst._immediates[0])._rel_32, inst.branch_target());
}

bool write_bcx_pseudo(const MetaInst& inst, std::ostream& sink, char const* form) {
  const uint8_t bi = inst._binst.bi_val();
  char const* flags_str = opcode_flags_string(inst._op, inst._binst);

  switch (bo_type_from_imm(inst._binst.bo())) {
    case BOType::kDnzf:
      sink << fmt::format("bdnzf{}{} {}", form, flags_str, disasm_crbit(bi));
      return true;

    case BOType::kDzf:
      sink << fmt::format("bdzf{}{} {}", form, flags_str, disasm_crbit(bi));
      return true;

    case BOType::kF:
      if (bi >= 4) {
        sink << fmt::format("b{}{}{} cr{}", condition_string_inv(bi), form, flags_str, bi / 4);
        return true;
      } else {
        sink << fmt::format("b{}{}{}", condition_string_inv(bi), form, flags_str);
        return false;
      }

    case BOType::kDnzt:
      sink << fmt::format("bdnzt{}{} {}", form, flags_str, disasm_crbit(bi));
      return true;

    case BOType::kDzt:
      sink << fmt::format("bdzt{}{} {}", form, flags_str, disasm_crbit(bi));
      return true;

    case BOType::kT:
      if (bi >= 4) {
        sink << fmt::format("b{}{}{} cr{}", condition_string(bi), form, flags_str, bi / 4);
        return true;
      } else {
        sink << fmt::format("b{}{}{}", condition_string(bi), form, flags_str);
        return false;
      }
      break;

    case BOType::kDnz:
      sink << fmt::format("bdnz{}{}", form, flags_str);
      return false;

    case BOType::kDz:
      sink << fmt::format("bdz{}{}", form, flags_str);
      return false;

    case BOType::kAlways:
      sink << fmt::format("b{}{}", form, flags_str);
      return false;

    default:
      return false;
  }
}

bool guess_rlwinm(const MetaInst& inst, std::ostream& sink) {
  uint8_t sh = inst._binst.sh()._val, mb = inst._binst.mb()._val, me = inst._binst.me()._val;
  const char* flags_str = opcode_flags_string(inst._op, inst._binst);

  // If shift is 0, the intent is likely a clrxwi, but not guaranteed
  if (sh == 0) {
    if (mb == 0) {
      uint8_t n = 31 - me;
      sink << fmt::format(
        "clrrwi{} {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n);
      return true;
    } else if (me == 31) {
      uint8_t n = mb;
      sink << fmt::format(
        "clrlwi{} {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n);
      return true;
    }
  }

  // Best fits either a rotrwi or rotlwi, so always disassemble it as that
  if (mb == 0 && me == 31) {
    if (sh >= 16) {
      uint8_t n = 32 - sh;
      sink << fmt::format(
        "rotrwi{} {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n);
    } else {
      uint8_t n = sh;
      sink << fmt::format(
        "rotlwi{} {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n);
    }
    return true;
  }

  // Best fits either a slwi or extlwi, so always disassemble it as that
  if (mb == 0) {
    if (sh + me == 31) {
      uint8_t n = 31 - me;
      sink << fmt::format(
        "slwi{} {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n);
    } else {
      uint8_t b = sh;
      uint8_t n = me + 1;
      sink << fmt::format(
        "extlwi{} {}, {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n, b);
    }
    return true;
  }

  // Bets fits either a srwi or extrwi, so always disassemble it as that
  if (me == 31) {
    if (sh + mb == 32) {
      uint8_t n = mb;
      sink << fmt::format(
        "srwi{} {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n);
    } else {
      uint8_t n = 32 - mb;
      uint8_t b = sh - n;
      sink << fmt::format(
        "extrwi{} {}, {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n, b);
    }
    return true;
  }

  if (sh + me == 31) {
    uint8_t n = sh;
    uint8_t b = mb + n;
    if (n <= b && b <= 31) {
      sink << fmt::format(
        "clrlslwi{} {}, {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n, b);
      return true;
    }
  }

  return false;
}

bool guess_rlwimi(const MetaInst& inst, std::ostream& sink) {
  uint8_t sh = inst._binst.sh()._val, mb = inst._binst.mb()._val, me = inst._binst.me()._val;
  const char* flags_str = opcode_flags_string(inst._op, inst._binst);

  // When SH and MB sum to 32, it can be interpreted as inslwi
  if (sh + mb == 32) {
    uint8_t b = mb;
    uint8_t n = me + 1 - b;
    sink << fmt::format(
      "inslwi{} {}, {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n, b);
    return true;
  }

  // When SH and ME sum to 31, it can be interpreted as insrwi
  if (sh + me == 31) {
    uint8_t b = mb;
    uint8_t n = me + 1 - b;
    sink << fmt::format(
      "insrwi{} {}, {}, {}, {}", flags_str, disasm_reg(inst._binst.ra()), disasm_reg(inst._binst.rs()), n, b);
    return true;
  }

  return false;
}

bool handle_pseudo_inst(const MetaInst& inst, std::ostream& sink) {
  switch (inst._op) {
    case InstOperation::kBc:
      if (write_bcx_pseudo(inst, sink, "")) {
        sink << ", ";
      } else {
        sink << " ";
      }
      write_rel_loc(inst, sink);
      return true;

    case InstOperation::kBclr:
      write_bcx_pseudo(inst, sink, "lr");
      return true;

    case InstOperation::kBcctr:
      write_bcx_pseudo(inst, sink, "ctr");
      return true;

    case InstOperation::kCmp:
      sink << fmt::format("cmp{} ", inst._binst.l_val() ? "d" : "w");
      if (inst._binst.crfd_val() == 0) {
        write_inst_operands<_Ra, _Rb>(inst, sink);
      } else {
        write_inst_operands<_Crfd, _Ra, _Rb>(inst, sink);
      }
      return true;

    case InstOperation::kCmpi:
      sink << fmt::format("cmp{}i ", inst._binst.l_val() ? "d" : "w");
      if (inst._binst.crfd_val() == 0) {
        write_inst_operands<_Ra, _Simm>(inst, sink);
      } else {
        write_inst_operands<_Crfd, _Ra, _Simm>(inst, sink);
      }
      return true;

    case InstOperation::kCmpl:
      sink << fmt::format("cmpl{} ", inst._binst.l_val() ? "d" : "w");
      if (inst._binst.crfd_val() == 0) {
        write_inst_operands<_Ra, _Rb>(inst, sink);
      } else {
        write_inst_operands<_Crfd, _Ra, _Rb>(inst, sink);
      }
      return true;

    case InstOperation::kCmpli:
      sink << fmt::format("cmpl{}i ", inst._binst.l_val() ? "d" : "w");
      if (inst._binst.crfd_val() == 0) {
        write_inst_operands<_Ra, _Simm>(inst, sink);
      } else {
        write_inst_operands<_Crfd, _Ra, _Simm>(inst, sink);
      }
      return true;

    case InstOperation::kOri:
      if (inst._binst.rd() == GPR::kR0 && inst._binst.ra() == GPR::kR0 && inst._binst.rb() == GPR::kR0) {
        sink << "nop";
        return true;
      }
      return false;

    case InstOperation::kAddi:
      if (inst._binst.ra() == GPR::kR0) {
        sink << "li ";
        write_inst_operands<_Rd, _Simm>(inst, sink);
        return true;
      }
      return false;

    case InstOperation::kAddis:
      if (inst._binst.ra() == GPR::kR0) {
        sink << "lis ";
        write_inst_operands<_Rd, _Simm>(inst, sink);
        return true;
      }
      return false;

    case InstOperation::kOr:
      if (inst._binst.rs() == inst._binst.rb()) {
        sink << fmt::format("mr{} ", opcode_flags_string(InstOperation::kOr, inst._binst));
        write_inst_operands<_Ra, _Rb>(inst, sink);
        return true;
      }
      return false;

    case InstOperation::kNor:
      if (inst._binst.rs() == inst._binst.rb()) {
        sink << fmt::format("not{} ", opcode_flags_string(InstOperation::kNor, inst._binst));
        write_inst_operands<_Ra, _Rb>(inst, sink);
        return true;
      }
      return false;

    case InstOperation::kMtcrf:
      if (inst._binst.crm_val() == 0xff) {
        sink << fmt::format("mtcr {}", disasm_reg(inst._binst.rs()));
        return true;
      }
      return false;

    case InstOperation::kRlwinm:
      return guess_rlwinm(inst, sink);

    case InstOperation::kRlwimi:
      return guess_rlwimi(inst, sink);

    case InstOperation::kRlwnm:
      if (inst._binst.mb()._val == 0 && inst._binst.me()._val == 31) {
        sink << fmt::format("rotlw{} ", opcode_flags_string(inst._op, inst._binst));
        write_inst_operands<_Ra, _Rs, _Rb>(inst, sink);
        return true;
      }
      return false;

    default:
      return false;
  }
}
}  // namespace

void write_inst_disassembly(const MetaInst& inst, std::ostream& sink) {
  if (handle_pseudo_inst(inst, sink)) {
    return;
  }

  sink << fmt::format("{}{} ", opcode_name(inst._op), opcode_flags_string(inst._op, inst._binst));

  switch (inst._op) {
    case InstOperation::kTw:
      write_inst_operands<_To, _Ra, _Rb>(inst, sink);
      break;

    case InstOperation::kTwi:
      write_inst_operands<_To, _Ra, _Simm>(inst, sink);
      break;

    case InstOperation::kRlwimi:
    case InstOperation::kRlwinm:
      write_inst_operands<_Ra, _Rs, _Sh, _Mb, _Me>(inst, sink);
      break;

    case InstOperation::kRlwnm:
      write_inst_operands<_Ra, _Rs, _Rb, _Mb, _Me>(inst, sink);
      break;

    case InstOperation::kLswi:
      write_inst_operands<_Rd, _Ra, _Nb>(inst, sink);
      break;

    case InstOperation::kStswi:
      write_inst_operands<_Rs, _Ra, _Nb>(inst, sink);
      break;

    case InstOperation::kLbz:
    case InstOperation::kLbzu:
    case InstOperation::kLha:
    case InstOperation::kLhau:
    case InstOperation::kLhz:
    case InstOperation::kLhzu:
    case InstOperation::kLmw:
    case InstOperation::kLwz:
    case InstOperation::kLwzu:
      write_inst_operands<_Rd, _D16ra>(inst, sink);
      break;

    case InstOperation::kLfd:
    case InstOperation::kLfdu:
    case InstOperation::kLfs:
    case InstOperation::kLfsu:
      write_inst_operands<_Frd, _D16ra>(inst, sink);
      break;

    case InstOperation::kLfdux:
    case InstOperation::kLfdx:
    case InstOperation::kLfsux:
    case InstOperation::kLfsx:
      write_inst_operands<_Frd, _Ra, _Rb>(inst, sink);
      break;

    case InstOperation::kAdd:
    case InstOperation::kAddc:
    case InstOperation::kAdde:
    case InstOperation::kDivw:
    case InstOperation::kDivwu:
    case InstOperation::kEciwx:
    case InstOperation::kLbzux:
    case InstOperation::kLbzx:
    case InstOperation::kLhaux:
    case InstOperation::kLhax:
    case InstOperation::kLhbrx:
    case InstOperation::kLhzux:
    case InstOperation::kLhzx:
    case InstOperation::kLswx:
    case InstOperation::kLwarx:
    case InstOperation::kLwbrx:
    case InstOperation::kLwzux:
    case InstOperation::kLwzx:
    case InstOperation::kMulhw:
    case InstOperation::kMulhwu:
    case InstOperation::kMullw:
    case InstOperation::kSubf:
    case InstOperation::kSubfc:
    case InstOperation::kSubfe:
      write_inst_operands<_Rd, _Ra, _Rb>(inst, sink);
      break;

    case InstOperation::kEcowx:
    case InstOperation::kStbux:
    case InstOperation::kStbx:
    case InstOperation::kSthbrx:
    case InstOperation::kSthux:
    case InstOperation::kSthx:
    case InstOperation::kStswx:
    case InstOperation::kStwbrx:
    case InstOperation::kStwcxDot:
    case InstOperation::kStwux:
    case InstOperation::kStwx:
      write_inst_operands<_Rs, _Ra, _Rb>(inst, sink);
      break;

    case InstOperation::kAddi:
    case InstOperation::kAddic:
    case InstOperation::kAddicDot:
    case InstOperation::kAddis:
    case InstOperation::kMulli:
    case InstOperation::kSubfic:
      write_inst_operands<_Rd, _Ra, _Simm>(inst, sink);
      break;

    case InstOperation::kAddme:
    case InstOperation::kAddze:
    case InstOperation::kNeg:
    case InstOperation::kSubfme:
    case InstOperation::kSubfze:
      write_inst_operands<_Rd, _Ra>(inst, sink);
      break;

    case InstOperation::kAnd:
    case InstOperation::kAndc:
    case InstOperation::kEqv:
    case InstOperation::kNand:
    case InstOperation::kNor:
    case InstOperation::kOr:
    case InstOperation::kOrc:
    case InstOperation::kSlw:
    case InstOperation::kSraw:
    case InstOperation::kSrw:
    case InstOperation::kXor:
      write_inst_operands<_Ra, _Rs, _Rb>(inst, sink);
      break;

    case InstOperation::kStb:
    case InstOperation::kStbu:
    case InstOperation::kSth:
    case InstOperation::kSthu:
    case InstOperation::kStmw:
    case InstOperation::kStw:
    case InstOperation::kStwu:
      write_inst_operands<_Rs, _D16ra>(inst, sink);
      break;

    case InstOperation::kSrawi:
      write_inst_operands<_Ra, _Rs, _Sh>(inst, sink);
      break;

    case InstOperation::kAndiDot:
    case InstOperation::kAndisDot:
    case InstOperation::kOri:
    case InstOperation::kOris:
    case InstOperation::kXori:
    case InstOperation::kXoris:
      write_inst_operands<_Ra, _Rs, _Uimm>(inst, sink);
      break;

    case InstOperation::kPsq_l:
    case InstOperation::kPsq_lu:
      write_inst_operands<_Frd, _D20ra, _W, _I17>(inst, sink);
      break;

    case InstOperation::kPsq_lux:
    case InstOperation::kPsq_lx:
      write_inst_operands<_Frd, _Ra, _Rb, _W, _I22>(inst, sink);
      break;

    case InstOperation::kPsq_st:
    case InstOperation::kPsq_stu:
      write_inst_operands<_Frs, _D20ra, _W, _I17>(inst, sink);
      break;

    case InstOperation::kPsq_stux:
    case InstOperation::kPsq_stx:
      write_inst_operands<_Frs, _Ra, _Rb, _W, _I22>(inst, sink);
      break;

    case InstOperation::kStfd:
    case InstOperation::kStfdu:
    case InstOperation::kStfs:
    case InstOperation::kStfsu:
      write_inst_operands<_Frs, _D16ra>(inst, sink);
      break;

    case InstOperation::kStfdux:
    case InstOperation::kStfdx:
    case InstOperation::kStfiwx:
    case InstOperation::kStfsux:
    case InstOperation::kStfsx:
      write_inst_operands<_Frs, _Ra, _Rb>(inst, sink);
      break;

    case InstOperation::kCntlzw:
    case InstOperation::kExtsb:
    case InstOperation::kExtsh:
      write_inst_operands<_Ra, _Rs>(inst, sink);
      break;

    case InstOperation::kDcbf:
    case InstOperation::kDcbi:
    case InstOperation::kDcbst:
    case InstOperation::kDcbt:
    case InstOperation::kDcbtst:
    case InstOperation::kDcbz:
    case InstOperation::kDcbz_l:
    case InstOperation::kIcbi:
      write_inst_operands<_Ra, _Rb>(inst, sink);
      break;

    case InstOperation::kMfsrin:
      write_inst_operands<_Rd, _Rb>(inst, sink);
      break;

    case InstOperation::kMtsrin:
      write_inst_operands<_Rs, _Rb>(inst, sink);
      break;

    case InstOperation::kMftb:
      write_inst_operands<_Rd, _Tbr>(inst, sink);
      break;

    case InstOperation::kMtmsr:
      write_inst_operands<_Rs>(inst, sink);
      break;

    case InstOperation::kMtspr:
      write_inst_operands<_Spr, _Rs>(inst, sink);
      break;

    case InstOperation::kMtsr:
      write_inst_operands<_Sr, _Rs>(inst, sink);
      break;

    case InstOperation::kMtcrf:
      write_inst_operands<_Crm, _Rs>(inst, sink);
      break;

    case InstOperation::kMtfsb0:
    case InstOperation::kMtfsb1:
      write_inst_operands<_Crbd>(inst, sink);
      break;

    case InstOperation::kMtfsf:
      write_inst_operands<_Fm, _Frb>(inst, sink);
      break;

    case InstOperation::kMtfsfi:
      write_inst_operands<_Crfd, _Imm>(inst, sink);
      break;

    case InstOperation::kTlbie:
      write_inst_operands<_Rb>(inst, sink);
      break;

    case InstOperation::kFadd:
    case InstOperation::kFadds:
    case InstOperation::kFdiv:
    case InstOperation::kFdivs:
    case InstOperation::kFsub:
    case InstOperation::kFsubs:
    case InstOperation::kPs_add:
    case InstOperation::kPs_div:
    case InstOperation::kPs_merge00:
    case InstOperation::kPs_merge01:
    case InstOperation::kPs_merge10:
    case InstOperation::kPs_merge11:
    case InstOperation::kPs_sub:
      write_inst_operands<_Frd, _Fra, _Frb>(inst, sink);
      break;

    case InstOperation::kFmadd:
    case InstOperation::kFmadds:
    case InstOperation::kFmsub:
    case InstOperation::kFmsubs:
    case InstOperation::kFnmadd:
    case InstOperation::kFnmadds:
    case InstOperation::kFnmsub:
    case InstOperation::kFnmsubs:
    case InstOperation::kFsel:
    case InstOperation::kPs_madd:
    case InstOperation::kPs_madds0:
    case InstOperation::kPs_madds1:
    case InstOperation::kPs_msub:
    case InstOperation::kPs_nmadd:
    case InstOperation::kPs_nmsub:
    case InstOperation::kPs_sel:
    case InstOperation::kPs_sum0:
    case InstOperation::kPs_sum1:
      write_inst_operands<_Frd, _Fra, _Frc, _Frb>(inst, sink);
      break;

    case InstOperation::kFmul:
    case InstOperation::kFmuls:
    case InstOperation::kPs_mul:
    case InstOperation::kPs_muls0:
    case InstOperation::kPs_muls1:
      write_inst_operands<_Frd, _Fra, _Frc>(inst, sink);
      break;

    case InstOperation::kFabs:
    case InstOperation::kFctiw:
    case InstOperation::kFctiwz:
    case InstOperation::kFmr:
    case InstOperation::kFnabs:
    case InstOperation::kFneg:
    case InstOperation::kFres:
    case InstOperation::kFrsp:
    case InstOperation::kFrsqrte:
    case InstOperation::kPs_abs:
    case InstOperation::kPs_mr:
    case InstOperation::kPs_nabs:
    case InstOperation::kPs_neg:
    case InstOperation::kPs_res:
    case InstOperation::kPs_rsqrte:
      write_inst_operands<_Frd, _Frb>(inst, sink);
      break;

    case InstOperation::kFcmpo:
    case InstOperation::kFcmpu:
    case InstOperation::kPs_cmpo0:
    case InstOperation::kPs_cmpo1:
    case InstOperation::kPs_cmpu0:
    case InstOperation::kPs_cmpu1:
      write_inst_operands<_Crfd, _Fra, _Frb>(inst, sink);
      break;

    case InstOperation::kMcrf:
    case InstOperation::kMcrfs:
      write_inst_operands<_Crfd, _Crfs>(inst, sink);
      break;

    case InstOperation::kMfspr:
      write_inst_operands<_Rd, _Spr>(inst, sink);
      break;

    case InstOperation::kMfsr:
      write_inst_operands<_Rd, _Sr>(inst, sink);
      break;

    case InstOperation::kMfcr:
    case InstOperation::kMfmsr:
      write_inst_operands<_Rd>(inst, sink);
      break;

    case InstOperation::kMffs:
      write_inst_operands<_Frd>(inst, sink);
      break;

    case InstOperation::kMcrxr:
      write_inst_operands<_Crfd>(inst, sink);
      break;

    case InstOperation::kCrand:
    case InstOperation::kCrandc:
    case InstOperation::kCreqv:
    case InstOperation::kCrnand:
    case InstOperation::kCrnor:
    case InstOperation::kCror:
    case InstOperation::kCrorc:
    case InstOperation::kCrxor:
      write_inst_operands<_Crbd, _Crba, _Crbb>(inst, sink);
      break;

    case InstOperation::kB:
      write_rel_loc(inst, sink);
      break;

    default:
      break;
  }
}

/////////////////////
// write_inst_info //
/////////////////////

namespace {
std::string spr_name(SPR spr) {
  switch (spr) {
    case SPR::kLr:
      return "LR";
    case SPR::kCtr:
      return "CTR";
    case SPR::kXer:
      return "XER";
    default:
      return fmt::format("SPRG#{}", static_cast<int>(spr));
  }
}

std::string tbr_name(TBR tbr) {
  switch (tbr) {
    case TBR::kTbl:
      return "TBL";
    case TBR::kTbu:
      return "TBU";
    default:
      return fmt::format("Invalid TBR#{}", static_cast<int>(tbr));
  }
}

std::string datasource_verbose(DataSource const& src) {
  return std::visit(
    overloaded{[](GPRSlice gpr) -> std::string { return fmt::format("r{}", static_cast<int>(gpr._reg)); },
      [](FPRSlice fpr) -> std::string { return fmt::format("f{}", static_cast<int>(fpr._reg)); },
      [](CRBit bits) -> std::string { return fmt::format("cr<{:08x}>", static_cast<uint32_t>(bits)); },
      [](MemRegOff mem) -> std::string { return fmt::format("[r{} + {}]", static_cast<int>(mem._base), mem._offset); },
      [](MemRegReg mem) -> std::string {
        return fmt::format("[r{} + r{}]", static_cast<int>(mem._base), static_cast<int>(mem._offset));
      },
      [](SPR spr) -> std::string { return spr_name(spr); },
      [](TBR tbr) -> std::string { return tbr_name(tbr); },
      [](FPSCRBit bits) -> std::string { return fmt::format("fpscr<{:08x}>", static_cast<uint32_t>(bits)); }},
    src);
}

std::string immsource_verbose(ImmSource const& src) {
  return std::visit(overloaded{
                      [](SIMM simm) -> std::string { return fmt::format("signed {}", simm._imm_value); },
                      [](UIMM uimm) -> std::string { return fmt::format("unsigned {}", uimm._imm_value); },
                      [](RelBranch br) -> std::string { return fmt::format("rel32 {:#x}", br._rel_32); },
                      [](AuxImm aux) -> std::string { return fmt::format("aux {}", aux._val); },
                    },
    src);
}
}  // namespace

void write_inst_info(MetaInst const& inst, std::ostream& sink) {
  if (inst._op == InstOperation::kInvalid) {
    sink << "Invalid decode" << std::endl;
    return;
  }

  auto print_list = [&sink](auto const& list, auto cb) {
    if (list.empty()) {
      return;
    }
    sink << cb(list[0]);
    for (size_t i = 1; i < list.size(); i++) {
      sink << ", " << cb(list[i]);
    }
  };

  sink << fmt::format("Binst: {:x}", inst._binst._bytes);
  sink << "\nOperand: " << opcode_name(inst._op);
  sink << "\nReads: ";
  print_list(inst._reads, datasource_verbose);

  sink << "\nImmediates: ";
  print_list(inst._immediates, immsource_verbose);

  sink << "\nWrites: ";
  print_list(inst._writes, datasource_verbose);

  sink << fmt::format("\nFlags: {:b}", static_cast<uint32_t>(inst._flags));

  sink << std::endl;
}
}  // namespace decomp
