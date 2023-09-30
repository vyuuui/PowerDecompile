#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

#include "BinInst.hh"
#include "DataSource.hh"
#include "FlagsEnum.hh"
#include "utl/ReservedVector.hh"

namespace decomp {
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

struct MetaInst {
  BinInst _binst;
  uint32_t _va;
  // All data sources being read
  reserved_vector<DataSource, 3> _reads;
  // Instruction immediates
  reserved_vector<ImmSource, 3> _immediates;
  // Output location
  reserved_vector<DataSource, 2> _writes;

  InstOperation _op = InstOperation::kInvalid;
  InstFlags _flags = InstFlags::kNone;

  template <typename T>
  T get_read_op() const {
    for (auto& ds : _reads) {
      if (std::holds_alternative<T>(ds)) {
        return std::get<T>(ds);
      }
    }
    assert(false);
  }

  template <typename T>
  T get_imm_op() const {
    for (auto& is : _immediates) {
      if (std::holds_alternative<T>(is)) {
        return std::get<T>(is);
      }
    }
    assert(false);
  }

  template <typename T>
  T get_write_op() const {
    for (auto& ds : _reads) {
      if (std::holds_alternative<T>(ds)) {
        return std::get<T>(ds);
      }
    }
    assert(false);
  }

  bool is_blr() const;

  constexpr bool is_direct_branch() const { return _op == InstOperation::kB || _op == InstOperation::kBc; }
  uint32_t branch_target() const { return _va + std::get<RelBranch>(_immediates[0])._rel_32; }
};

void disasm_single(uint32_t vaddr, uint32_t raw_inst, MetaInst& meta_out);

BOType bo_type_from_imm(AuxImm imm);
}  // namespace decomp
