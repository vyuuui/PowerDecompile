#pragma once

#include <cstddef>
#include <cstdint>

#include "ReservedVector.hh"
#include "DataSource.hh"

// Operation as determined by the opcode and possible function code
enum class InstOperation {
  kAdd, kAddc, kAdde, kAddi, kAddic, kAddicDot, kAddis, kAddme,
  kAddze, kDivw, kDivwu, kMulhw, kMulhwu, kMulli, kMullw, kNeg,
  kSubf, kSubfc, kSubfe, kSubfic, kSubfme, kSubfze, kCmp, kCmpi,
  kCmpl, kCmpli, kAnd, kAndc, kAndiDot, kAndisDot, kCntlzw, kEqv,
  kExtsb, kExtsh, kNand, kNor, kOr, kOrc, kOri, kOris,
  kXor, kXori, kXoris, kRlwimi, kRlwinm, kRlwnm, kSlw, kSraw,
  kSrawi, kSrw, kFadd, kFadds, kFdiv, kFdivs, kFmul, kFmuls,
  kFres, kFrsqrte, kFsub, kFsubs, kFsel, kFmadd, kFmadds, kFmsub,
  kFmsubs, kFnmadd, kFnmadds, kFnmsub, kFnmsubs, kFctiw, kFctiwz, kFrsp,
  kFcmpo, kFcmpu, kMcrfs, kMffs, kMtfsb0, kMtfsb1, kMtfsf, kMtfsfi,
  kLbz, kLbzu, kLbzux, kLbzx, kLha, kLhau, kLhaux, kLhax,
  kLhz, kLhzu, kLhzux, kLhzx, kLwz, kLwzu, kLwzux, kLwzx,
  kStb, kStbu, kStbux, kStbx, kSth, kSthu, kSthux, kSthx,
  kStw, kStwu, kStwux, kStwx, kLhbrx, kLwbrx, kSthbrx, kStwbrx,
  kLmw, kStmw, kLswi, kLswx, kStswi, kStswx, kEieio, kIsync,
  kLwarx, kStwcxDot, kSync, kLfd, kLfdu, kLfdux, kLfdx, kLfs,
  kLfsu, kLfsux, kLfsx, kStfd, kStfdu, kStfdux, kStfdx, kStfiwx,
  kStfs, kStfsu, kStfsux, kStfsx, kFabs, kFmr, kFnabs, kFneg,
  kB, kBc, kBcctr, kBclr, kCrand, kCrandc, kCreqv, kCrnand,
  kCrnor, kCror, kCrorc, kCrxor, kMcrf, kRfi, kSc, kTw,
  kTwi, kMcrxr, kMfcr, kMfmsr, kMfspr, kMftb, kMtcrf, kMtmsr,
  kMtspr, kDcbf, kDcbi, kDcbst, kDcbt, kDcbtst, kDcbz, kIcbi,
  kMfsr, kMfsrin, kMtsr, kMtsrin, kTlbie, kTlbsync, kEciwx, kEcowx,
  kPsq_lx, kPsq_stx, kPsq_lux, kPsq_stux, kPsq_l, kPsq_lu, kPsq_st, kPsq_stu,
  kPs_div, kPs_sub, kPs_add, kPs_sel, kPs_res, kPs_mul, kPs_rsqrte, kPs_msub,
  kPs_madd, kPs_nmsub, kPs_nmadd, kPs_neg, kPs_mr, kPs_nabs, kPs_abs, kPs_sum0,
  kPs_sum1, kPs_muls0, kPs_muls1, kPs_madds0, kPs_madds1, kPs_cmpu0, kPs_cmpo0, kPs_cmpu1,
  kPs_cmpo1, kPs_merge00, kPs_merge01, kPs_merge10, kPs_merge11, kDcbz_l,
};

union BinInst {
  struct {
    uint32_t _opcode : 6;
    uint32_t _rest : 26;
  };
  uint32_t _raw;
};

enum class InstFlags : uint32_t {
  kNone         = 0b0000,
  kAll          = 0b1111,
  kWritesCR0    = 0b0001,
  kWritesXER    = 0b0010,
  kWritesLR     = 0b0100,
  kAbsoluteAddr = 0b1000,
};

constexpr InstFlags operator|(InstFlags lhs, InstFlags rhs) {
  return static_cast<InstFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}
constexpr InstFlags operator&(InstFlags lhs, InstFlags rhs) {
  return static_cast<InstFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}
constexpr InstFlags operator^(InstFlags lhs, InstFlags rhs) {
  return static_cast<InstFlags>(static_cast<uint32_t>(lhs) ^ static_cast<uint32_t>(rhs));
}
constexpr InstFlags operator~(InstFlags flags) {
  return flags ^ InstFlags::kAll;
}
constexpr bool check_flags(InstFlags check, InstFlags against) {
  return (check & against & InstFlags::kAll) != InstFlags::kNone;
}
constexpr bool any_flags(InstFlags flags) {
  return (flags & InstFlags::kAll) != InstFlags::kNone;
}

struct MetaInst {
  reserved_vector<DataSource, 3> _reads;
  DataSource _writes;
  InstFlags _flags;
};
