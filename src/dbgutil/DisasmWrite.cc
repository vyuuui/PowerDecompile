#include "dbgutil/DisasmWrite.hh"

#include <fmt/format.h>

#include <iostream>
#include <string>
#include <variant>

#include "PpcDisasm.hh"

namespace decomp {
namespace {
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

std::string operation_name(InstOperation op) {
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

std::string datasource_to_string(DataSource const& src) {
  return std::visit(
      overloaded{[](GPR gpr) -> std::string { return fmt::format("r{}", static_cast<int>(gpr)); },
                 [](FPR fpr) -> std::string { return fmt::format("f{}", static_cast<int>(fpr)); },
                 [](CRBit bits) -> std::string {
                   return fmt::format("cr<{:08x}>", static_cast<uint32_t>(bits));
                 },
                 [](MemRegOff mem) -> std::string {
                   return fmt::format("[r{} + {}]", static_cast<int>(mem._base), mem._offset);
                 },
                 [](MemRegReg mem) -> std::string {
                   return fmt::format("[r{} + r{}]", static_cast<int>(mem._base),
                                      static_cast<int>(mem._offset));
                 },
                 [](SPR spr) -> std::string { return spr_name(spr); },
                 [](TBR tbr) -> std::string { return tbr_name(tbr); },
                 [](FPSCRBit bits) -> std::string {
                   return fmt::format("fpscr<{:08x}>", static_cast<uint32_t>(bits));
                 }},
      src);
}

std::string immsource_to_string(ImmSource const& src) {
  return std::visit(
      overloaded{
          [](SIMM simm) -> std::string { return fmt::format("signed {}", simm._imm_value); },
          [](UIMM uimm) -> std::string { return fmt::format("unsigned {}", uimm._imm_value); },
          [](RelBranch br) -> std::string { return fmt::format("rel32 {:x}", br._rel_32); },
          [](AuxImm aux) -> std::string { return fmt::format("aux {}", aux._val); },
      },
      src);
}
}  // namespace

void write_disassembly(MetaInst const& inst, std::ostream& sink) {
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

  sink << fmt::format("Binst: {:08x}", inst._binst);
  sink << "\nOperand: " << operation_name(inst._op);
  sink << "\nReads: ";
  print_list(inst._reads, datasource_to_string);

  sink << "\nImmediates: ";
  print_list(inst._immediates, immsource_to_string);

  sink << "\nWrites: ";
  print_list(inst._writes, datasource_to_string);

  sink << fmt::format("\nFlags: {:b}", static_cast<uint32_t>(inst._flags));

  sink << std::endl;
}
}  // namespace decomp
