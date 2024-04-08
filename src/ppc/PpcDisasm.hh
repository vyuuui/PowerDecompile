#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

#include "ppc/BinInst.hh"
#include "ppc/DataSource.hh"
#include "utl/FlagsEnum.hh"
#include "utl/ReservedVector.hh"

namespace decomp::ppc {
// Operation as determined by the opcode and possible function code
enum class InstOperation {
  kAdd,
  kAddc,
  kAdde,
  kAddi,
  kAddic,
  kAddicDot,
  kAddis,
  kAddme,
  kAddze,
  kDivw,
  kDivwu,
  kMulhw,
  kMulhwu,
  kMulli,
  kMullw,
  kNeg,
  kSubf,
  kSubfc,
  kSubfe,
  kSubfic,
  kSubfme,
  kSubfze,
  kCmp,
  kCmpi,
  kCmpl,
  kCmpli,
  kAnd,
  kAndc,
  kAndiDot,
  kAndisDot,
  kCntlzw,
  kEqv,
  kExtsb,
  kExtsh,
  kNand,
  kNor,
  kOr,
  kOrc,
  kOri,
  kOris,
  kXor,
  kXori,
  kXoris,
  kRlwimi,
  kRlwinm,
  kRlwnm,
  kSlw,
  kSraw,
  kSrawi,
  kSrw,
  kFadd,
  kFadds,
  kFdiv,
  kFdivs,
  kFmul,
  kFmuls,
  kFres,
  kFrsqrte,
  kFsub,
  kFsubs,
  kFsel,
  kFmadd,
  kFmadds,
  kFmsub,
  kFmsubs,
  kFnmadd,
  kFnmadds,
  kFnmsub,
  kFnmsubs,
  kFctiw,
  kFctiwz,
  kFrsp,
  kFcmpo,
  kFcmpu,
  kMcrfs,
  kMffs,
  kMtfsb0,
  kMtfsb1,
  kMtfsf,
  kMtfsfi,
  kLbz,
  kLbzu,
  kLbzux,
  kLbzx,
  kLha,
  kLhau,
  kLhaux,
  kLhax,
  kLhz,
  kLhzu,
  kLhzux,
  kLhzx,
  kLwz,
  kLwzu,
  kLwzux,
  kLwzx,
  kStb,
  kStbu,
  kStbux,
  kStbx,
  kSth,
  kSthu,
  kSthux,
  kSthx,
  kStw,
  kStwu,
  kStwux,
  kStwx,
  kLhbrx,
  kLwbrx,
  kSthbrx,
  kStwbrx,
  kLmw,
  kStmw,
  kLswi,
  kLswx,
  kStswi,
  kStswx,
  kEieio,
  kIsync,
  kLwarx,
  kStwcxDot,
  kSync,
  kLfd,
  kLfdu,
  kLfdux,
  kLfdx,
  kLfs,
  kLfsu,
  kLfsux,
  kLfsx,
  kStfd,
  kStfdu,
  kStfdux,
  kStfdx,
  kStfiwx,
  kStfs,
  kStfsu,
  kStfsux,
  kStfsx,
  kFabs,
  kFmr,
  kFnabs,
  kFneg,
  kB,
  kBc,
  kBcctr,
  kBclr,
  kCrand,
  kCrandc,
  kCreqv,
  kCrnand,
  kCrnor,
  kCror,
  kCrorc,
  kCrxor,
  kMcrf,
  kRfi,
  kSc,
  kTw,
  kTwi,
  kMcrxr,
  kMfcr,
  kMfmsr,
  kMfspr,
  kMftb,
  kMtcrf,
  kMtmsr,
  kMtspr,
  kDcbf,
  kDcbi,
  kDcbst,
  kDcbt,
  kDcbtst,
  kDcbz,
  kIcbi,
  kMfsr,
  kMfsrin,
  kMtsr,
  kMtsrin,
  kTlbie,
  kTlbsync,
  kEciwx,
  kEcowx,
  kPsq_lx,
  kPsq_stx,
  kPsq_lux,
  kPsq_stux,
  kPsq_l,
  kPsq_lu,
  kPsq_st,
  kPsq_stu,
  kPs_div,
  kPs_sub,
  kPs_add,
  kPs_sel,
  kPs_res,
  kPs_mul,
  kPs_rsqrte,
  kPs_msub,
  kPs_madd,
  kPs_nmsub,
  kPs_nmadd,
  kPs_neg,
  kPs_mr,
  kPs_nabs,
  kPs_abs,
  kPs_sum0,
  kPs_sum1,
  kPs_muls0,
  kPs_muls1,
  kPs_madds0,
  kPs_madds1,
  kPs_cmpu0,
  kPs_cmpo0,
  kPs_cmpu1,
  kPs_cmpo1,
  kPs_merge00,
  kPs_merge01,
  kPs_merge10,
  kPs_merge11,
  kDcbz_l,
  kInvalid,
};

// BO (branch operation?) type, mapped from instruction encoding to mnemonic
enum class BOType { kDnzf, kDzf, kF, kDnzt, kDzt, kT, kDnz, kDz, kAlways, kInvalid };
enum class RlwimiOp { kNone, kInslwi, kInsrwi };
struct SimplifiedRlwimi {
  RlwimiOp _op;
  uint8_t _n;
  uint8_t _b;
};

enum class RlwinmOp { kNone, kExtlwi, kExtrwi, kRotlwi, kRotrwi, kSlwi, kSrwi, kClrlwi, kClrrwi, kClrlslwi };
struct SimplifiedRlwinm {
  RlwinmOp _op;
  uint8_t _n;
  uint8_t _b;
};

enum class RlwnmOp { kNone, kRotlw };
struct SimplifiedRlwnm {
  RlwnmOp _op;
};

struct MetaInst {
  BinInst _binst;
  uint32_t _va;
  // All data sources being read
  // Sources are ordered based on the operation being done if said operation is not commutative
  reserved_vector<DataSource, 4> _reads;
  // Output location
  std::optional<DataSource> _write;

  InstOperation _op = InstOperation::kInvalid;
  InstSideFx _sidefx = InstSideFx::kNone;
  InstFlags _flags = InstFlags::kNone;
  FPSCRBit _fpcrsidefx = FPSCRBit::kNone;

  template <typename T>
  T get_read_op() const {
    for (auto& ds : _reads) {
      if (std::holds_alternative<T>(ds)) {
        return std::get<T>(ds);
      }
    }
    assert(false);
  }

  bool is_blr() const;

  constexpr bool is_direct_branch() const { return _op == InstOperation::kB || _op == InstOperation::kBc; }
  constexpr uint32_t branch_target() const {
    const uint32_t base = check_flags(_flags, InstFlags::kAbsoluteAddr) ? 0 : _va;
    if (_op == InstOperation::kB) {
      return _va + _binst.li()._rel_32;
    } else if (_op == InstOperation::kBc) {
      return _va + _binst.bd()._rel_32;
    }
    return 0;
  }

  SimplifiedRlwimi simplified_rlwimi() const;
  SimplifiedRlwinm simplified_rlwinm() const;
  SimplifiedRlwnm simplified_rlwnm() const;
};

void disasm_single(uint32_t vaddr, uint32_t raw_inst, MetaInst& meta_out);

BOType bo_type_from_imm(AuxImm imm);
}  // namespace decomp::ppc
