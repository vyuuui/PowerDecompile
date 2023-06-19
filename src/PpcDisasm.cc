#include "PpcDisasm.hh"

#include <cstddef>
#include <cstdint>
#include <iostream>

#include "DataSource.hh"
#include "ReservedVector.hh"

namespace decomp {
namespace {
constexpr uint32_t gen_mask(uint32_t left, uint32_t right) {
  return static_cast<uint32_t>(((uint64_t{1} << (32 - left)) - 1) &
                               ~((uint64_t{1} << (31 - right)) - 1));
}

struct BinInst {
public:
  uint32_t _bytes;

public:
  constexpr uint32_t ext_range(uint32_t left, uint32_t right) const {
    return (_bytes & gen_mask(left, right)) >> (31 - right);
  }
  constexpr int32_t ext_range_signed(uint32_t left, uint32_t right) const {
    if ((_bytes >> (31 - left)) & 1) {
      return static_cast<int32_t>(gen_mask(0, left + (31 - right)) | ext_range(left, right));
    }
    return static_cast<int32_t>(ext_range(left, right));
  }

  // Bitswapped fields
  constexpr SPR spr() const {
    return static_cast<SPR>(((ext_range(11, 20) >> 5) & 0b0000011111) |
                            ((ext_range(11, 20) << 5) & 0b1111100000));
  }
  constexpr TBR tbr() const {
    return static_cast<TBR>(((ext_range(11, 20) >> 5) & 0b0000011111) |
                            ((ext_range(11, 20) << 5) & 0b1111100000));
  }

  // Normal fields
  // Short relative branch distance
  constexpr RelBranch bd() const { return RelBranch{ext_range_signed(16, 29) << 2}; }
  // Bit to use for conditional branch
  constexpr CRBit bi() const { return static_cast<CRBit>(1 << ext_range(11, 15)); }
  // Branch options for conditional branch
  constexpr AuxImm bo() const { return AuxImm{ext_range(6, 10)}; }
  // CR bit numbers
  constexpr CRBit crba() const { return static_cast<CRBit>(1 << ext_range(11, 15)); }
  constexpr CRBit crbb() const { return static_cast<CRBit>(1 << ext_range(16, 20)); }
  constexpr CRBit crbd() const { return static_cast<CRBit>(1 << ext_range(6, 10)); }
  // CR field numbers
  constexpr CRBit crfd() const { return static_cast<CRBit>(0b1111u << ext_range(6, 8)); }
  constexpr CRBit crfs() const { return static_cast<CRBit>(0b1111u << ext_range(11, 13)); }
  // Floating point registers
  constexpr FPR fra() const { return static_cast<FPR>(ext_range(11, 15)); }
  constexpr FPR frb() const { return static_cast<FPR>(ext_range(16, 20)); }
  constexpr FPR frc() const { return static_cast<FPR>(ext_range(21, 25)); }
  constexpr FPR frd() const { return static_cast<FPR>(ext_range(6, 10)); }
  constexpr FPR frs() const { return static_cast<FPR>(ext_range(6, 10)); }
  // GQR control register
  constexpr AuxImm i17() const { return AuxImm{ext_range(17, 19)}; }
  constexpr AuxImm i22() const { return AuxImm{ext_range(22, 24)}; }
  // Imm to fill FPSCR
  constexpr AuxImm imm() const { return AuxImm{ext_range(16, 19)}; }
  // Long relative branch distance
  constexpr RelBranch li() const { return RelBranch{ext_range_signed(6, 29) << 2}; }
  // Bitmask begin and end index
  constexpr AuxImm mb() const { return AuxImm{ext_range(21, 25)}; }
  constexpr AuxImm me() const { return AuxImm{ext_range(26, 30)}; }
  // Number of bytes for string load/store
  constexpr AuxImm nb() const { return AuxImm{ext_range(16, 20)}; }
  // General purpose registers
  constexpr GPR ra() const { return static_cast<GPR>(ext_range(11, 15)); }
  constexpr GPR rb() const { return static_cast<GPR>(ext_range(16, 20)); }
  constexpr GPR rd() const { return static_cast<GPR>(ext_range(6, 10)); }
  constexpr GPR rs() const { return static_cast<GPR>(ext_range(6, 10)); }
  // Shift for bitmask defined by MB and ME
  constexpr AuxImm sh() const { return AuxImm{ext_range(16, 20)}; }
  // Signed 16-bit immediate
  constexpr SIMM simm() const { return SIMM{ext_range_signed(16, 31)}; }
  // Segment register number
  constexpr AuxImm sr() const { return AuxImm{ext_range(12, 15)}; }
  // Trap operation for tw/twi
  constexpr AuxImm to() const { return AuxImm{ext_range(6, 10)}; }
  // Unsigned 16-bit immediate
  constexpr UIMM uimm() const { return UIMM{ext_range(16, 31)}; }

  // Flag fields
  constexpr InstFlags oe() const {
    return ext_range(30, 30) ? InstFlags::kWritesXER : InstFlags::kNone;
  }
  constexpr InstFlags rc() const {
    return ext_range(31, 31) ? InstFlags::kWritesRecord : InstFlags::kNone;
  }
  constexpr InstFlags aa() const {
    return ext_range(30, 30) ? InstFlags::kAbsoluteAddr : InstFlags::kNone;
  }
  constexpr InstFlags lk() const {
    return ext_range(31, 31) ? InstFlags::kWritesLR : InstFlags::kNone;
  }
  constexpr InstFlags w() const {
    return ext_range(21, 21) ? InstFlags::kPsLoadsOne : InstFlags::kNone;
  }
  constexpr InstFlags l() const {
    return ext_range(10, 10) ? InstFlags::kLongMode : InstFlags::kNone;
  }

  // Partially completed fields
  constexpr int32_t d16() const { return ext_range_signed(16, 31); }
  constexpr int32_t d20() const { return ext_range_signed(20, 31); }

  // Misc fields
  constexpr uint32_t opcd() const { return ext_range(0, 5); }
  constexpr FPSCRBit fpscrbd() const { return static_cast<FPSCRBit>(1 << ext_range(6, 10)); }
  constexpr FPSCRBit fpscrfd() const { return static_cast<FPSCRBit>(0b1111u << ext_range(6, 8)); }
  constexpr FPSCRBit fpscrfs() const { return static_cast<FPSCRBit>(0b1111u << ext_range(11, 13)); }

  // Mask for CR fields
  CRBit crm() const {
    CRBit result = CRBit::kNone;
    for (uint32_t i = 0; i < 8; i++) {
      if (ext_range(i + 12, i + 19)) {
        result = result | static_cast<CRBit>(0b1111u << (4 * i));
      }
    }
    return result;
  }

  // Mask for FPSCR fields
  FPSCRBit fm() const {
    FPSCRBit result = FPSCRBit::kNone;
    for (uint32_t i = 0; i < 8; i++) {
      if (ext_range(i + 7, i + 7)) {
        result = result | static_cast<FPSCRBit>(0b1111u << (4 * i));
      }
    }
    return result & FPSCRBit::kWriteMask;
  }
};

InstOperation op_for_psfunc(uint32_t op) {
  switch (op) {
    case 0:
      return InstOperation::kPs_cmpu0;
    case 6:
      return InstOperation::kPsq_lx;
    case 7:
      return InstOperation::kPsq_stx;
    case 10:
      return InstOperation::kPs_sum0;
    case 11:
      return InstOperation::kPs_sum1;
    case 12:
      return InstOperation::kPs_muls0;
    case 13:
      return InstOperation::kPs_muls1;
    case 14:
      return InstOperation::kPs_madds0;
    case 15:
      return InstOperation::kPs_madds1;
    case 18:
      return InstOperation::kPs_div;
    case 20:
      return InstOperation::kPs_sub;
    case 21:
      return InstOperation::kPs_add;
    case 23:
      return InstOperation::kPs_sel;
    case 24:
      return InstOperation::kPs_res;
    case 25:
      return InstOperation::kPs_mul;
    case 26:
      return InstOperation::kPs_rsqrte;
    case 28:
      return InstOperation::kPs_msub;
    case 29:
      return InstOperation::kPs_madd;
    case 30:
      return InstOperation::kPs_nmsub;
    case 31:
      return InstOperation::kPs_nmadd;
    case 32:
      return InstOperation::kPs_cmpo0;
    case 38:
      return InstOperation::kPsq_lux;
    case 39:
      return InstOperation::kPsq_stux;
    case 40:
      return InstOperation::kPs_neg;
    case 64:
      return InstOperation::kPs_cmpu1;
    case 72:
      return InstOperation::kPs_mr;
    case 96:
      return InstOperation::kPs_cmpo1;
    case 136:
      return InstOperation::kPs_nabs;
    case 264:
      return InstOperation::kPs_abs;
    case 528:
      return InstOperation::kPs_merge00;
    case 560:
      return InstOperation::kPs_merge01;
    case 592:
      return InstOperation::kPs_merge10;
    case 624:
      return InstOperation::kPs_merge11;
    case 1014:
      return InstOperation::kDcbz_l;
    default:
      return InstOperation::kInvalid;
  }
}

std::optional<FPSCRBit> fpscr_bits_for_psfunc(uint32_t op) {
  switch (op) {
    case 0:
    case 64:
      return FPSCRBit::kFpcc | FPSCRBit::kFx | FPSCRBit::kVxsnan;

    case 10:
    case 11:
    case 20:
    case 21:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxisi;

    case 12:
    case 13:
    case 14:
    case 15:
    case 28:
    case 29:
    case 30:
    case 31:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxisi |
             FPSCRBit::kVximz;

    case 18:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kZx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxidi |
             FPSCRBit::kVxzdz;

    case 24:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kZx | FPSCRBit::kVxsnan;

    case 25:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVximz;

    case 26:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kZx |
             FPSCRBit::kVxsnan | FPSCRBit::kVxsqrt;

    case 32:
    case 96:
      return FPSCRBit::kFpcc | FPSCRBit::kFx | FPSCRBit::kVxsnan | FPSCRBit::kVxvc;

    default:
      return std::nullopt;
  }
}

void disasm_opcode_4(BinInst binst, MetaInst& meta_out) {
  uint32_t psfunc = binst.ext_range(21, 30);
  bool search_again = false;
  switch (psfunc) {
    case 40:
    case 72:
    case 136:
    case 264:
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 0:
    case 32:
    case 64:
    case 96:
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.crfd());
      break;
    case 528:
    case 560:
    case 592:
    case 624:
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 1014:
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    default:
      search_again = false;
      break;
  }

  if (search_again) {
    search_again = false;
    switch (psfunc & 0b111111) {
      case 6:
      case 7:
      case 38:
      case 39:
        meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
        meta_out._immediates.push_back(binst.i22());
        meta_out._writes.push_back(binst.frd());
        meta_out._flags = binst.w();
        break;
      default:
        search_again = true;
        break;
    }
    psfunc &= 0b111111;
  }

  if (search_again) {
    search_again = false;
    switch (psfunc & 0b11111) {
      case 18:
      case 20:
      case 21:
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._writes.push_back(binst.frd());
        meta_out._flags = binst.rc();
        break;
      case 23:
      case 28:
      case 29:
      case 30:
      case 31:
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._reads.push_back(binst.frc());
        meta_out._writes.push_back(binst.frd());
        meta_out._flags = binst.rc();
        break;
      case 24:
      case 26:
        meta_out._reads.push_back(binst.frb());
        meta_out._writes.push_back(binst.frd());
        meta_out._flags = binst.rc();
        break;
      case 25:
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frc());
        meta_out._writes.push_back(binst.frd());
        meta_out._flags = binst.rc();
        break;
      default:
        search_again = true;
        break;
    }
    psfunc &= 0b11111;
  }

  meta_out._op = op_for_psfunc(psfunc);
  std::optional<FPSCRBit> bits = fpscr_bits_for_psfunc(psfunc);
  if (bits) {
    meta_out._writes.push_back(*bits);
  }
}

void disasm_opcode_19(BinInst binst, MetaInst& meta_out) {
  const uint32_t crfunc = binst.ext_range(21, 30);
  constexpr auto fill_crbit_binop = [](BinInst binst, MetaInst& meta_out) {
    meta_out._reads.push_back(binst.crba());
    meta_out._reads.push_back(binst.crbb());
    meta_out._writes.push_back(binst.crbd());
  };

  switch (crfunc) {
    case 0:
      meta_out._op = InstOperation::kMcrf;
      meta_out._reads.push_back(binst.crfd());
      meta_out._writes.push_back(binst.crfd());
      break;
    case 16:
      meta_out._op = InstOperation::kBclr;
      meta_out._reads.push_back(binst.bi());
      meta_out._reads.push_back(SPR::kLr);
      meta_out._immediates.push_back(binst.bo());
      if ((binst.bo()._val & 0b00100) == 0) {
        meta_out._writes.push_back(SPR::kCtr);
      }
      meta_out._flags = binst.lk();
      break;
    case 33:
      meta_out._op = InstOperation::kCrnor;
      fill_crbit_binop(binst, meta_out);
      break;
    case 50:
      meta_out._op = InstOperation::kRfi;
      break;
    case 129:
      meta_out._op = InstOperation::kCrandc;
      fill_crbit_binop(binst, meta_out);
      break;
    case 150:
      meta_out._op = InstOperation::kIsync;
      break;
    case 193:
      meta_out._op = InstOperation::kCrxor;
      fill_crbit_binop(binst, meta_out);
      break;
    case 225:
      meta_out._op = InstOperation::kCrnand;
      fill_crbit_binop(binst, meta_out);
      break;
    case 257:
      meta_out._op = InstOperation::kCrand;
      fill_crbit_binop(binst, meta_out);
      break;
    case 417:
      meta_out._op = InstOperation::kCrorc;
      fill_crbit_binop(binst, meta_out);
      break;
    case 449:
      meta_out._op = InstOperation::kCror;
      fill_crbit_binop(binst, meta_out);
      break;
    case 528:
      meta_out._op = InstOperation::kBcctr;
      meta_out._reads.push_back(binst.bi());
      meta_out._reads.push_back(SPR::kCtr);
      meta_out._immediates.push_back(binst.bo());
      meta_out._flags = binst.lk();
      break;
    default:
      meta_out._op = InstOperation::kInvalid;
      break;
  }
}

void disasm_opcode_31(BinInst binst, MetaInst& meta_out) {
  const uint32_t arith_func = binst.ext_range(21, 30);
  bool not_found = false;
  switch (arith_func) {
    case 0:
      meta_out._op = InstOperation::kCmp;
      meta_out._reads.push_back(binst.ra());
      meta_out._reads.push_back(binst.rb());
      meta_out._reads.push_back(SPR::kXer);
      meta_out._writes.push_back(binst.crfd());
      meta_out._flags = binst.l();
      break;
    case 4:
      meta_out._op = InstOperation::kTw;
      meta_out._reads.push_back(binst.ra());
      meta_out._reads.push_back(binst.rb());
      meta_out._immediates.push_back(binst.to());
      break;
    case 11:
      meta_out._op = InstOperation::kMulhwu;
      meta_out._reads.push_back(binst.ra());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.rd());
      meta_out._flags = binst.rc();
      break;
    case 19:
      meta_out._op = InstOperation::kMfcr;
      meta_out._writes.push_back(binst.rd());
      break;
    case 20:
      meta_out._op = InstOperation::kLwarx;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      break;
    case 23:
      meta_out._op = InstOperation::kLwzx;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      break;
    case 24:
      meta_out._op = InstOperation::kSlw;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 26:
      meta_out._op = InstOperation::kCntlzw;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 28:
      meta_out._op = InstOperation::kAnd;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 32:
      meta_out._op = InstOperation::kCmpl;
      meta_out._reads.push_back(binst.ra());
      meta_out._reads.push_back(binst.rb());
      meta_out._reads.push_back(SPR::kXer);
      meta_out._writes.push_back(binst.crfd());
      meta_out._flags = binst.l();
      break;
    case 54:
      meta_out._op = InstOperation::kDcbst;
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 60:
      meta_out._op = InstOperation::kAndc;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 75:
      meta_out._op = InstOperation::kMulhw;
      meta_out._reads.push_back(binst.ra());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.rd());
      meta_out._flags = binst.rc();
      break;
    case 83:
      meta_out._op = InstOperation::kMfmsr;
      meta_out._writes.push_back(binst.rd());
      break;
    case 86:
      meta_out._op = InstOperation::kDcbf;
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 87:
      meta_out._op = InstOperation::kLbzx;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      break;
    case 119:
      meta_out._op = InstOperation::kLbzux;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      meta_out._writes.push_back(binst.ra());
      break;
    case 124:
      meta_out._op = InstOperation::kNor;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 144:
      meta_out._op = InstOperation::kMtcrf;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(binst.crm());
      break;
    case 146:
      meta_out._op = InstOperation::kMtmsr;
      meta_out._reads.push_back(binst.rs());
      break;
    case 150:
      meta_out._op = InstOperation::kStwcxDot;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(SPR::kXer);
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._flags = InstFlags::kWritesRecord;
      break;
    case 151:
      meta_out._op = InstOperation::kStwx;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 183:
      meta_out._op = InstOperation::kStwux;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.ra());
      break;
    case 210:
      meta_out._op = InstOperation::kMtsr;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.sr());
      break;
    case 215:
      meta_out._op = InstOperation::kStbx;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 242:
      meta_out._op = InstOperation::kMtsrin;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      break;
    case 246:
      meta_out._op = InstOperation::kDcbtst;
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 247:
      meta_out._op = InstOperation::kStbux;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rb());
      break;
    case 278:
      meta_out._op = InstOperation::kDcbt;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 279:
      meta_out._op = InstOperation::kLhzx;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      break;
    case 284:
      meta_out._op = InstOperation::kEqv;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 306:
      meta_out._op = InstOperation::kTlbie;
      meta_out._reads.push_back(binst.rb());
      break;
    case 310:
      meta_out._op = InstOperation::kEciwx;
      meta_out._reads.push_back(binst.ra());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.rd());
      break;
    case 311:
      meta_out._op = InstOperation::kLhzux;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._reads.push_back(binst.rd());
      break;
    case 316:
      meta_out._op = InstOperation::kXor;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 339:
      meta_out._op = InstOperation::kMfspr;
      meta_out._reads.push_back(binst.spr());
      meta_out._writes.push_back(binst.rd());
      break;
    case 343:
      meta_out._op = InstOperation::kLhax;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      break;
    case 371:
      meta_out._op = InstOperation::kMftb;
      meta_out._reads.push_back(binst.tbr());
      meta_out._writes.push_back(binst.rd());
      break;
    case 375:
      meta_out._op = InstOperation::kLhaux;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      meta_out._writes.push_back(binst.ra());
      break;
    case 407:
      meta_out._op = InstOperation::kSthx;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 412:
      meta_out._op = InstOperation::kOrc;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 438:
      meta_out._op = InstOperation::kEcowx;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 439:
      meta_out._op = InstOperation::kSthux;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 444:
      meta_out._op = InstOperation::kOr;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 467:
      meta_out._op = InstOperation::kMtspr;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(binst.spr());
      break;
    case 470:
      meta_out._op = InstOperation::kDcbi;
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 476:
      meta_out._op = InstOperation::kNand;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 512:
      meta_out._op = InstOperation::kMcrxr;
      meta_out._reads.push_back(SPR::kXer);
      meta_out._writes.push_back(binst.crfd());
      meta_out._writes.push_back(SPR::kXer);
      break;
    case 533:
      meta_out._op = InstOperation::kLswx;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      break;
    case 534:
      meta_out._op = InstOperation::kLwbrx;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      break;
    case 535:
      meta_out._op = InstOperation::kLfsx;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.frd());
      break;
    case 536:
      meta_out._op = InstOperation::kSrw;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 566:
      meta_out._op = InstOperation::kTlbsync;
      break;
    case 567:
      meta_out._op = InstOperation::kLfsux;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.frd());
      meta_out._writes.push_back(binst.ra());
      break;
    case 595:
      meta_out._op = InstOperation::kMfsr;
      meta_out._immediates.push_back(binst.sr());
      meta_out._writes.push_back(binst.rd());
      break;
    case 597:
      meta_out._op = InstOperation::kLswi;
      meta_out._reads.push_back(MemRegOff{binst.ra(), 0});
      meta_out._immediates.push_back(binst.nb());
      meta_out._writes.push_back(binst.rd());
      break;
    case 598:
      meta_out._op = InstOperation::kSync;
      break;
    case 599:
      meta_out._op = InstOperation::kLfdx;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.frd());
      break;
    case 631:
      meta_out._op = InstOperation::kLfdux;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.frd());
      meta_out._writes.push_back(binst.ra());
      break;
    case 659:
      meta_out._op = InstOperation::kMfsrin;
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.rd());
      break;
    case 661:
      meta_out._op = InstOperation::kStswx;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(SPR::kXer);
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 662:
      meta_out._op = InstOperation::kStwbrx;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 663:
      meta_out._op = InstOperation::kStfsx;
      meta_out._reads.push_back(binst.frs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 695:
      meta_out._op = InstOperation::kStfsux;
      meta_out._reads.push_back(binst.frs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.ra());
      break;
    case 725:
      meta_out._op = InstOperation::kStswi;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.nb());
      meta_out._writes.push_back(MemRegOff{binst.ra(), 0});
      break;
    case 727:
      meta_out._op = InstOperation::kStfdx;
      meta_out._reads.push_back(binst.frs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 759:
      meta_out._op = InstOperation::kStfdux;
      meta_out._reads.push_back(binst.frs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.ra());
      break;
    case 790:
      meta_out._op = InstOperation::kLhbrx;
      meta_out._reads.push_back(MemRegReg{binst.ra(), binst.rb()});
      meta_out._writes.push_back(binst.rd());
      break;
    case 792:
      meta_out._op = InstOperation::kSraw;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._writes.push_back(binst.ra());
      meta_out._writes.push_back(SPR::kXer);
      meta_out._flags = binst.rc();
      break;
    case 824:
      meta_out._op = InstOperation::kSrawi;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.sh());
      meta_out._writes.push_back(binst.ra());
      meta_out._writes.push_back(SPR::kXer);
      meta_out._flags = binst.rc();
      break;
    case 854:
      meta_out._op = InstOperation::kEieio;
      break;
    case 918:
      meta_out._op = InstOperation::kSthbrx;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 922:
      meta_out._op = InstOperation::kExtsh;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 954:
      meta_out._op = InstOperation::kExtsb;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;
    case 982:
      meta_out._op = InstOperation::kIcbi;
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 983:
      meta_out._op = InstOperation::kStfiwx;
      meta_out._reads.push_back(binst.frs());
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    case 1014:
      meta_out._op = InstOperation::kDcbz;
      meta_out._writes.push_back(MemRegReg{binst.ra(), binst.rb()});
      break;
    default:
      not_found = true;
      break;
  }

  if (not_found) {
    switch (arith_func & 0b111111111) {
      case 8:
        meta_out._op = InstOperation::kSubfc;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(binst.rb());
        meta_out._writes.push_back(binst.rd());
        meta_out._writes.push_back(SPR::kXer);
        break;
      case 10:
        meta_out._op = InstOperation::kAddc;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(binst.rb());
        meta_out._writes.push_back(binst.rd());
        meta_out._writes.push_back(SPR::kXer);
        break;
      case 40:
        meta_out._op = InstOperation::kSubf;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(binst.rb());
        meta_out._writes.push_back(binst.rd());
        break;
      case 104:
        meta_out._op = InstOperation::kNeg;
        meta_out._reads.push_back(binst.ra());
        meta_out._writes.push_back(binst.rd());
        break;
      case 136:
        meta_out._op = InstOperation::kSubfe;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(binst.rb());
        meta_out._reads.push_back(SPR::kXer);
        meta_out._writes.push_back(binst.rd());
        meta_out._writes.push_back(SPR::kXer);
        break;
      case 138:
        meta_out._op = InstOperation::kAdde;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(binst.rb());
        meta_out._reads.push_back(SPR::kXer);
        meta_out._writes.push_back(binst.rd());
        meta_out._writes.push_back(SPR::kXer);
        break;
      case 200:
        meta_out._op = InstOperation::kSubfze;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(SPR::kXer);
        meta_out._writes.push_back(binst.rd());
        meta_out._writes.push_back(SPR::kXer);
        break;
      case 202:
        meta_out._op = InstOperation::kAddze;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(SPR::kXer);
        meta_out._writes.push_back(binst.rd());
        meta_out._writes.push_back(SPR::kXer);
        break;
      case 232:
        meta_out._op = InstOperation::kSubfme;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(SPR::kXer);
        meta_out._writes.push_back(binst.rd());
        meta_out._writes.push_back(SPR::kXer);
        break;
      case 234:
        meta_out._op = InstOperation::kAddme;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(SPR::kXer);
        meta_out._writes.push_back(binst.rd());
        meta_out._writes.push_back(SPR::kXer);
        break;
      case 235:
        meta_out._op = InstOperation::kMullw;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(binst.rb());
        meta_out._writes.push_back(binst.rd());
        break;
      case 266:
        meta_out._op = InstOperation::kAdd;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(binst.rb());
        meta_out._writes.push_back(binst.rd());
        break;
      case 459:
        meta_out._op = InstOperation::kDivwu;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(binst.rb());
        meta_out._writes.push_back(binst.rd());
        break;
      case 491:
        meta_out._op = InstOperation::kDivw;
        meta_out._reads.push_back(binst.ra());
        meta_out._reads.push_back(binst.rb());
        meta_out._writes.push_back(binst.rd());
        break;
      default:
        meta_out._op = InstOperation::kInvalid;
        break;
    }
    meta_out._flags = binst.rc() | binst.oe();
  }
}

std::optional<FPSCRBit> fpscr_bits_for_fs_func(uint32_t func) {
  switch (func) {
    case 18:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kZx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxidi |
             FPSCRBit::kVxzdz;
    case 20:
    case 21:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxisi;
    case 24:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kZx | FPSCRBit::kVxsnan;
    case 25:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxisi;
    case 28:
    case 29:
    case 30:
    case 31:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxisi |
             FPSCRBit::kVximz;
    default:
      return std::nullopt;
  }
}

void disasm_opcode_59(BinInst binst, MetaInst& meta_out) {
  const uint32_t float_single_func = binst.ext_range(26, 30);
  switch (float_single_func) {
    case 18:
      meta_out._op = InstOperation::kFdivs;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      break;
    case 20:
      meta_out._op = InstOperation::kFsubs;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      break;
    case 21:
      meta_out._op = InstOperation::kFadds;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      break;
    case 24:
      meta_out._op = InstOperation::kFres;
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      break;
    case 25:
      meta_out._op = InstOperation::kFmuls;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frc());
      meta_out._writes.push_back(binst.frd());
      break;
    case 28:
      meta_out._op = InstOperation::kFmsubs;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._reads.push_back(binst.frc());
      meta_out._writes.push_back(binst.frd());
      break;
    case 29:
      meta_out._op = InstOperation::kFmadds;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._reads.push_back(binst.frc());
      meta_out._writes.push_back(binst.frd());
      break;
    case 30:
      meta_out._op = InstOperation::kFnmsubs;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._reads.push_back(binst.frc());
      meta_out._writes.push_back(binst.frd());
      break;
    case 31:
      meta_out._op = InstOperation::kFnmadds;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._reads.push_back(binst.frc());
      meta_out._writes.push_back(binst.frd());
      break;
    default:
      meta_out._op = InstOperation::kInvalid;
      break;
  }
  meta_out._flags = binst.rc();
  std::optional<FPSCRBit> bits = fpscr_bits_for_fs_func(float_single_func);
  if (bits) {
    meta_out._writes.push_back(*bits);
  }
}

std::optional<FPSCRBit> fpscr_bits_for_fd_func(uint32_t func) {
  switch (func) {
    case 0:
      return FPSCRBit::kFpcc | FPSCRBit::kVxsnan;

    case 12:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan;

    case 14:
    case 15:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kXx |
             FPSCRBit::kVxsnan | FPSCRBit::kVxcvi;

    case 18:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kZx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxidi |
             FPSCRBit::kVxzdz;

    case 20:
    case 21:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxisi;

    case 25:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVximz;

    case 26:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kZx |
             FPSCRBit::kVxsnan | FPSCRBit::kVxsqrt;

    case 28:
    case 29:
    case 30:
    case 31:
      return FPSCRBit::kFprf | FPSCRBit::kFr | FPSCRBit::kFi | FPSCRBit::kFx | FPSCRBit::kOx |
             FPSCRBit::kUx | FPSCRBit::kXx | FPSCRBit::kVxsnan | FPSCRBit::kVxisi |
             FPSCRBit::kVximz;

    case 32:
      return FPSCRBit::kFpcc | FPSCRBit::kFx | FPSCRBit::kVxsnan | FPSCRBit::kVxvc;

    default:
      return std::nullopt;
  }
}

void disasm_opcode_63(BinInst binst, MetaInst& meta_out) {
  uint32_t float_double_func = binst.ext_range(21, 30);
  bool not_found = false;
  switch (float_double_func) {
    case 0:
      meta_out._op = InstOperation::kFcmpu;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.crfd());
      break;
    case 12:
      meta_out._op = InstOperation::kFrsp;
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 14:
      meta_out._op = InstOperation::kFctiw;
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 15:
      meta_out._op = InstOperation::kFctiwz;
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 32:
      meta_out._op = InstOperation::kFcmpo;
      meta_out._reads.push_back(binst.fra());
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.crfd());
      break;
    case 38:
      meta_out._op = InstOperation::kMtfsb1;
      meta_out._writes.push_back(binst.fpscrbd() | FPSCRBit::kFx);
      meta_out._flags = binst.rc();
      break;
    case 40:
      meta_out._op = InstOperation::kFneg;
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 64: {
      meta_out._op = InstOperation::kMcrfs;
      meta_out._reads.push_back(binst.fpscrfs());
      meta_out._writes.push_back(binst.crfd());
      meta_out._writes.push_back(binst.fpscrfs() & FPSCRBit::kExceptionMask);
      break;
    }
    case 70:
      meta_out._op = InstOperation::kMtfsb0;
      meta_out._writes.push_back(binst.fpscrbd());
      meta_out._flags = binst.rc();
      break;
    case 72:
      meta_out._op = InstOperation::kFmr;
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 134:
      meta_out._op = InstOperation::kMtfsfi;
      meta_out._immediates.push_back(binst.imm());
      meta_out._writes.push_back(binst.fpscrfd() & FPSCRBit::kWriteMask);
      meta_out._flags = binst.rc();
      break;
    case 136:
      meta_out._op = InstOperation::kFnabs;
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 264:
      meta_out._op = InstOperation::kFabs;
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 583:
      meta_out._op = InstOperation::kMffs;
      meta_out._reads.push_back(FPSCRBit::kAll);
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.rc();
      break;
    case 711:
      meta_out._op = InstOperation::kMtfsf;
      meta_out._reads.push_back(binst.frb());
      meta_out._writes.push_back(binst.fm());
      meta_out._flags = binst.rc();
      break;
    default:
      not_found = true;
      break;
  }

  if (not_found) {
    float_double_func &= 0b11111;
    switch (float_double_func) {
      case 18:
        meta_out._op = InstOperation::kFdiv;
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._writes.push_back(binst.frd());
        break;
      case 20:
        meta_out._op = InstOperation::kFsub;
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._writes.push_back(binst.frd());
        break;
      case 21:
        meta_out._op = InstOperation::kFadd;
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._writes.push_back(binst.frd());
        break;
      case 23:
        meta_out._op = InstOperation::kFsel;
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._reads.push_back(binst.frc());
        meta_out._writes.push_back(binst.frd());
        break;
      case 25:
        meta_out._op = InstOperation::kFmul;
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frc());
        meta_out._writes.push_back(binst.frd());
        break;
      case 26:
        meta_out._op = InstOperation::kFrsqrte;
        meta_out._reads.push_back(binst.frb());
        meta_out._writes.push_back(binst.frd());
        break;
      case 28:
        meta_out._op = InstOperation::kFmsub;
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._reads.push_back(binst.frc());
        meta_out._writes.push_back(binst.frd());
        break;
      case 29:
        meta_out._op = InstOperation::kFmadd;
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._reads.push_back(binst.frc());
        meta_out._writes.push_back(binst.frd());
        break;
      case 30:
        meta_out._op = InstOperation::kFnmsub;
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._reads.push_back(binst.frc());
        meta_out._writes.push_back(binst.frd());
        break;
      case 31:
        meta_out._op = InstOperation::kFnmadd;
        meta_out._reads.push_back(binst.fra());
        meta_out._reads.push_back(binst.frb());
        meta_out._reads.push_back(binst.frc());
        meta_out._writes.push_back(binst.frd());
        break;
      default:
        meta_out._op = InstOperation::kInvalid;
        break;
    }

    meta_out._flags = binst.rc();
  }

  std::optional<FPSCRBit> bits = fpscr_bits_for_fd_func(float_double_func);
  if (bits) {
    meta_out._writes.push_back(*bits);
  }
}
}  // namespace

void disasm_single(uint32_t raw_inst, MetaInst& meta_out) {
  const BinInst binst{raw_inst};
  meta_out._binst = binst._bytes;

  switch (binst.opcd()) {
    case 3:
      meta_out._op = InstOperation::kTwi;
      meta_out._immediates.push_back(binst.to());
      meta_out._reads.push_back(binst.ra());
      meta_out._immediates.push_back(binst.simm());
      break;

    case 4:
      disasm_opcode_4(binst, meta_out);
      break;

    case 7:
      meta_out._op = InstOperation::kMulli;
      meta_out._reads.push_back(binst.ra());
      meta_out._immediates.push_back(binst.simm());
      meta_out._writes.push_back(binst.rd());
      break;

    case 8:
      meta_out._op = InstOperation::kSubfic;
      meta_out._reads.push_back(binst.ra());
      meta_out._immediates.push_back(binst.simm());
      meta_out._writes.push_back(binst.rd());
      meta_out._writes.push_back(SPR::kXer);
      break;

    case 10:
      meta_out._op = InstOperation::kCmpli;
      meta_out._reads.push_back(binst.ra());
      meta_out._reads.push_back(SPR::kXer);
      meta_out._immediates.push_back(binst.uimm());
      meta_out._writes.push_back(binst.crfd());
      meta_out._flags = binst.l();
      break;

    case 11:
      meta_out._op = InstOperation::kCmpi;
      meta_out._reads.push_back(binst.ra());
      meta_out._reads.push_back(SPR::kXer);
      meta_out._immediates.push_back(binst.simm());
      meta_out._writes.push_back(binst.crfd());
      meta_out._flags = binst.l();
      break;

    case 12:
      meta_out._op = InstOperation::kAddic;
      meta_out._reads.push_back(binst.ra());
      meta_out._immediates.push_back(binst.simm());
      meta_out._writes.push_back(binst.rd());
      meta_out._writes.push_back(SPR::kXer);
      break;

    case 13:
      meta_out._op = InstOperation::kAddicDot;
      meta_out._reads.push_back(binst.ra());
      meta_out._immediates.push_back(binst.simm());
      meta_out._writes.push_back(binst.rd());
      meta_out._writes.push_back(SPR::kXer);
      meta_out._flags = InstFlags::kWritesRecord;
      break;

    case 14:
      meta_out._op = InstOperation::kAddi;
      if (binst.ra() != GPR::kR0) {
        meta_out._reads.push_back(binst.ra());
      } else {
        meta_out._immediates.push_back(AuxImm{0});
      }
      meta_out._immediates.push_back(binst.simm());
      meta_out._writes.push_back(binst.rd());
      break;

    case 15:
      meta_out._op = InstOperation::kAddis;
      if (binst.ra() != GPR::kR0) {
        meta_out._reads.push_back(binst.ra());
      } else {
        meta_out._immediates.push_back(AuxImm{0});
      }
      meta_out._immediates.push_back(binst.simm());
      meta_out._writes.push_back(binst.rd());
      break;

    case 16:
      meta_out._op = InstOperation::kBc;
      meta_out._reads.push_back(binst.bi());
      meta_out._immediates.push_back(binst.bo());
      meta_out._immediates.push_back(binst.bd());
      if ((binst.bo()._val & 0b00100) == 0) {
        meta_out._writes.push_back(SPR::kCtr);
      }
      meta_out._flags = binst.aa() | binst.lk();
      break;

    case 17:
      meta_out._op = InstOperation::kSc;
      break;

    case 18:
      meta_out._op = InstOperation::kB;
      meta_out._immediates.push_back(binst.li());
      meta_out._flags = binst.aa() | binst.lk();
      break;

    case 19:
      disasm_opcode_19(binst, meta_out);
      break;

    case 20:
      meta_out._op = InstOperation::kRlwimi;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.sh());
      meta_out._immediates.push_back(binst.mb());
      meta_out._immediates.push_back(binst.me());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;

    case 21:
      meta_out._op = InstOperation::kRlwinm;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.sh());
      meta_out._immediates.push_back(binst.mb());
      meta_out._immediates.push_back(binst.me());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;

    case 23:
      meta_out._op = InstOperation::kRlwnm;
      meta_out._reads.push_back(binst.rs());
      meta_out._reads.push_back(binst.rb());
      meta_out._immediates.push_back(binst.mb());
      meta_out._immediates.push_back(binst.me());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.rc();
      break;

    case 24:
      meta_out._op = InstOperation::kOri;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.uimm());
      meta_out._writes.push_back(binst.ra());
      break;

    case 25:
      meta_out._op = InstOperation::kOris;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.uimm());
      meta_out._writes.push_back(binst.ra());
      break;

    case 26:
      meta_out._op = InstOperation::kXori;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.uimm());
      meta_out._writes.push_back(binst.ra());
      break;

    case 27:
      meta_out._op = InstOperation::kXoris;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.uimm());
      meta_out._writes.push_back(binst.ra());
      break;

    case 28:
      meta_out._op = InstOperation::kAndiDot;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.uimm());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = InstFlags::kWritesRecord;
      break;

    case 29:
      meta_out._op = InstOperation::kAndisDot;
      meta_out._reads.push_back(binst.rs());
      meta_out._immediates.push_back(binst.uimm());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = InstFlags::kWritesRecord;
      break;

    case 31:
      disasm_opcode_31(binst, meta_out);
      break;

    case 32:
      meta_out._op = InstOperation::kLwz;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.rd());
      break;

    case 33:
      meta_out._op = InstOperation::kLwzu;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.rd());
      meta_out._writes.push_back(binst.ra());
      break;

    case 34:
      meta_out._op = InstOperation::kLbz;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.rd());
      break;

    case 35:
      meta_out._op = InstOperation::kLbzu;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.rd());
      meta_out._writes.push_back(binst.ra());
      break;

    case 36:
      meta_out._op = InstOperation::kStw;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      break;

    case 37:
      meta_out._op = InstOperation::kStwu;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.ra());
      break;

    case 38:
      meta_out._op = InstOperation::kStb;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      break;

    case 39:
      meta_out._op = InstOperation::kStbu;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.ra());
      break;

    case 40:
      meta_out._op = InstOperation::kLhz;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.rd());
      break;

    case 41:
      meta_out._op = InstOperation::kLhzu;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.rd());
      meta_out._writes.push_back(binst.ra());
      break;

    case 42:
      meta_out._op = InstOperation::kLha;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.rd());
      break;

    case 43:
      meta_out._op = InstOperation::kLhau;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.rd());
      meta_out._writes.push_back(binst.ra());
      break;

    case 44:
      meta_out._op = InstOperation::kSth;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      break;

    case 45:
      meta_out._op = InstOperation::kSthu;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.ra());
      break;

    case 46:
      meta_out._op = InstOperation::kLmw;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.rd());
      break;

    case 47:
      meta_out._op = InstOperation::kStmw;
      meta_out._reads.push_back(binst.rs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      break;

    case 48:
      meta_out._op = InstOperation::kLfs;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.frd());
      break;

    case 49:
      meta_out._op = InstOperation::kLfsu;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.frd());
      meta_out._writes.push_back(binst.ra());
      break;

    case 50:
      meta_out._op = InstOperation::kLfd;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.frd());
      break;

    case 51:
      meta_out._op = InstOperation::kLfdu;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.frd());
      meta_out._writes.push_back(binst.ra());
      break;

    case 52:
      meta_out._op = InstOperation::kStfs;
      meta_out._reads.push_back(binst.frs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      break;

    case 53:
      meta_out._op = InstOperation::kStfsu;
      meta_out._reads.push_back(binst.frs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.ra());
      break;

    case 54:
      meta_out._op = InstOperation::kStfd;
      meta_out._reads.push_back(binst.frs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      break;

    case 55:
      meta_out._op = InstOperation::kStfdu;
      meta_out._reads.push_back(binst.frs());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d16()});
      meta_out._writes.push_back(binst.ra());
      break;

    case 56:
      meta_out._op = InstOperation::kPsq_l;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d20()});
      meta_out._immediates.push_back(binst.i17());
      meta_out._writes.push_back(binst.frd());
      meta_out._flags = binst.w();
      break;

    case 57:
      meta_out._op = InstOperation::kPsq_lu;
      meta_out._reads.push_back(MemRegOff{binst.ra(), binst.d20()});
      meta_out._immediates.push_back(binst.i17());
      meta_out._writes.push_back(binst.frd());
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.w();
      break;

    case 59:
      disasm_opcode_59(binst, meta_out);
      break;

    case 60:
      meta_out._op = InstOperation::kPsq_st;
      meta_out._reads.push_back(binst.frs());
      meta_out._immediates.push_back(binst.i17());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d20()});
      meta_out._flags = binst.w();
      break;

    case 61:
      meta_out._op = InstOperation::kPsq_stu;
      meta_out._reads.push_back(binst.frs());
      meta_out._immediates.push_back(binst.i17());
      meta_out._writes.push_back(MemRegOff{binst.ra(), binst.d20()});
      meta_out._writes.push_back(binst.ra());
      meta_out._flags = binst.w();
      break;

    case 63:
      disasm_opcode_63(binst, meta_out);
      break;

    default:
      meta_out._op = InstOperation::kInvalid;
      break;
  }
}
}  // namespace decomp
