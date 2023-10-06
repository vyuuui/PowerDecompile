#pragma once

#include <cstdint>

#include "DataSource.hh"

namespace decomp {
constexpr uint32_t gen_mask(uint32_t left, uint32_t right) {
  return static_cast<uint32_t>(((uint64_t{1} << (32 - left)) - 1) & ~((uint64_t{1} << (31 - right)) - 1));
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
    return static_cast<SPR>(((ext_range(11, 20) >> 5) & 0b0000011111) | ((ext_range(11, 20) << 5) & 0b1111100000));
  }
  constexpr TBR tbr() const {
    return static_cast<TBR>(((ext_range(11, 20) >> 5) & 0b0000011111) | ((ext_range(11, 20) << 5) & 0b1111100000));
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
  constexpr FPRSlice fra_s() const { return FPRSlice{fra(), DataType::kSingle}; }
  constexpr FPRSlice frb_s() const { return FPRSlice{frb(), DataType::kSingle}; }
  constexpr FPRSlice frc_s() const { return FPRSlice{frc(), DataType::kSingle}; }
  constexpr FPRSlice frd_s() const { return FPRSlice{frd(), DataType::kSingle}; }
  constexpr FPRSlice frs_s() const { return FPRSlice{frs(), DataType::kSingle}; }
  constexpr FPRSlice fra_d() const { return FPRSlice{fra(), DataType::kDouble}; }
  constexpr FPRSlice frb_d() const { return FPRSlice{frb(), DataType::kDouble}; }
  constexpr FPRSlice frc_d() const { return FPRSlice{frc(), DataType::kDouble}; }
  constexpr FPRSlice frd_d() const { return FPRSlice{frd(), DataType::kDouble}; }
  constexpr FPRSlice frs_d() const { return FPRSlice{frs(), DataType::kDouble}; }
  constexpr FPRSlice fra_p() const { return FPRSlice{fra(), DataType::kPackedSingle}; }
  constexpr FPRSlice frb_p() const { return FPRSlice{frb(), DataType::kPackedSingle}; }
  constexpr FPRSlice frc_p() const { return FPRSlice{frc(), DataType::kPackedSingle}; }
  constexpr FPRSlice frd_p() const { return FPRSlice{frd(), DataType::kPackedSingle}; }
  constexpr FPRSlice frs_p() const { return FPRSlice{frs(), DataType::kPackedSingle}; }
  constexpr FPRSlice fra_v() const { return FPRSlice{fra(), DataType::kSingleOrDouble}; }
  constexpr FPRSlice frb_v() const { return FPRSlice{frb(), DataType::kSingleOrDouble}; }
  constexpr FPRSlice frc_v() const { return FPRSlice{frc(), DataType::kSingleOrDouble}; }
  constexpr FPRSlice frd_v() const { return FPRSlice{frd(), DataType::kSingleOrDouble}; }
  constexpr FPRSlice frs_v() const { return FPRSlice{frs(), DataType::kSingleOrDouble}; }
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
  constexpr GPRSlice ra_b() const { return GPRSlice{ra(), DataType::kS1}; }
  constexpr GPRSlice rb_b() const { return GPRSlice{rb(), DataType::kS1}; }
  constexpr GPRSlice rd_b() const { return GPRSlice{rd(), DataType::kS1}; }
  constexpr GPRSlice rs_b() const { return GPRSlice{rs(), DataType::kS1}; }
  constexpr GPRSlice ra_h() const { return GPRSlice{ra(), DataType::kS2}; }
  constexpr GPRSlice rb_h() const { return GPRSlice{rb(), DataType::kS2}; }
  constexpr GPRSlice rd_h() const { return GPRSlice{rd(), DataType::kS2}; }
  constexpr GPRSlice rs_h() const { return GPRSlice{rs(), DataType::kS2}; }
  constexpr GPRSlice ra_w() const { return GPRSlice{ra(), DataType::kS4}; }
  constexpr GPRSlice rb_w() const { return GPRSlice{rb(), DataType::kS4}; }
  constexpr GPRSlice rd_w() const { return GPRSlice{rd(), DataType::kS4}; }
  constexpr GPRSlice rs_w() const { return GPRSlice{rs(), DataType::kS4}; }
  // Shift for bitmask defined by MB and ME
  constexpr AuxImm sh() const { return AuxImm{ext_range(16, 20)}; }
  // Signed 16-bit immediate
  constexpr SIMM simm() const { return SIMM{static_cast<int16_t>(ext_range_signed(16, 31))}; }
  // Segment register number
  constexpr AuxImm sr() const { return AuxImm{ext_range(12, 15)}; }
  // Trap operation for tw/twi
  constexpr AuxImm to() const { return AuxImm{ext_range(6, 10)}; }
  // Unsigned 16-bit immediate
  constexpr UIMM uimm() const { return UIMM{static_cast<uint16_t>(ext_range(16, 31))}; }

  // Flag fields
  constexpr InstFlags oe() const { return ext_range(30, 30) ? InstFlags::kWritesXER : InstFlags::kNone; }
  constexpr InstFlags rc() const { return ext_range(31, 31) ? InstFlags::kWritesRecord : InstFlags::kNone; }
  constexpr InstFlags aa() const { return ext_range(30, 30) ? InstFlags::kAbsoluteAddr : InstFlags::kNone; }
  constexpr InstFlags lk() const { return ext_range(31, 31) ? InstFlags::kWritesLR : InstFlags::kNone; }
  constexpr InstFlags w() const { return ext_range(21, 21) ? InstFlags::kPsLoadsOne : InstFlags::kNone; }
  constexpr InstFlags l() const { return ext_range(10, 10) ? InstFlags::kLongMode : InstFlags::kNone; }

  // Partially completed fields
  constexpr int16_t d16() const { return ext_range_signed(16, 31); }
  constexpr int16_t d20() const { return ext_range_signed(20, 31); }

  // Misc fields
  constexpr uint32_t opcd() const { return ext_range(0, 5); }
  constexpr FPSCRBit fpscrbd() const { return static_cast<FPSCRBit>(1 << ext_range(6, 10)); }
  constexpr FPSCRBit fpscrfd() const { return static_cast<FPSCRBit>(0b1111u << ext_range(6, 8)); }
  constexpr FPSCRBit fpscrfs() const { return static_cast<FPSCRBit>(0b1111u << ext_range(11, 13)); }

  // Mask for CR fields
  CRBit crm() const {
    CRBit result = CRBit::kNone;
    for (uint32_t i = 0; i < 8; i++) {
      if (ext_range(i + 12, i + 12)) {
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

  // Raw Values as needed
  // CR bit values
  constexpr uint8_t crba_val() const { return ext_range(11, 15); }
  constexpr uint8_t crbb_val() const { return ext_range(16, 20); }
  constexpr uint8_t crbd_val() const { return ext_range(6, 10); }
  constexpr uint8_t bi_val() const { return ext_range(11, 15); }

  // CR field values
  constexpr uint8_t crfd_val() const { return ext_range(6, 8); }
  constexpr uint8_t crfs_val() const { return ext_range(11, 13); }

  // CR mask value
  constexpr uint8_t crm_val() const { return ext_range(12, 19); }

  // FPSCR mask value
  constexpr uint8_t fm_val() const { return ext_range(7, 14); }

  // PS flags
  constexpr uint8_t l_val() const { return ext_range(10, 10); }
  constexpr uint8_t w_val() const { return ext_range(21, 21); }
};
}  // namespace decomp
