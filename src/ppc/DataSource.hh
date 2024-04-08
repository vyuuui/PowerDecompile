#pragma once

#include <cstdint>
#include <variant>

#include "ppc/RegSet.hh"
#include "utl/FlagsEnum.hh"

namespace decomp::ppc {
enum class GPR : uint8_t {
  kR0,
  kR1,
  kR2,
  kR3,
  kR4,
  kR5,
  kR6,
  kR7,
  kR8,
  kR9,
  kR10,
  kR11,
  kR12,
  kR13,
  kR14,
  kR15,
  kR16,
  kR17,
  kR18,
  kR19,
  kR20,
  kR21,
  kR22,
  kR23,
  kR24,
  kR25,
  kR26,
  kR27,
  kR28,
  kR29,
  kR30,
  kR31,
};

constexpr bool operator>(GPR l, GPR r) { return static_cast<int>(l) > static_cast<int>(r); }
constexpr bool operator>=(GPR l, GPR r) { return static_cast<int>(l) >= static_cast<int>(r); }
constexpr bool operator<(GPR l, GPR r) { return static_cast<int>(l) < static_cast<int>(r); }
constexpr bool operator<=(GPR l, GPR r) { return static_cast<int>(l) <= static_cast<int>(r); }

enum class FPR : uint8_t {
  kF0,
  kF1,
  kF2,
  kF3,
  kF4,
  kF5,
  kF6,
  kF7,
  kF8,
  kF9,
  kF10,
  kF11,
  kF12,
  kF13,
  kF14,
  kF15,
  kF16,
  kF17,
  kF18,
  kF19,
  kF20,
  kF21,
  kF22,
  kF23,
  kF24,
  kF25,
  kF26,
  kF27,
  kF28,
  kF29,
  kF30,
  kF31,
};

constexpr bool operator>(FPR l, FPR r) { return static_cast<int>(l) > static_cast<int>(r); }
constexpr bool operator>=(FPR l, FPR r) { return static_cast<int>(l) >= static_cast<int>(r); }
constexpr bool operator<(FPR l, FPR r) { return static_cast<int>(l) < static_cast<int>(r); }
constexpr bool operator<=(FPR l, FPR r) { return static_cast<int>(l) <= static_cast<int>(r); }

enum class CRField : uint8_t {
  kCr0,
  kCr1,
  kCr2,
  kCr3,
  kCr4,
  kCr5,
  kCr6,
  kCr7,
};

constexpr bool operator>(CRField l, CRField r) { return static_cast<int>(l) > static_cast<int>(r); }
constexpr bool operator>=(CRField l, CRField r) { return static_cast<int>(l) >= static_cast<int>(r); }
constexpr bool operator<(CRField l, CRField r) { return static_cast<int>(l) < static_cast<int>(r); }
constexpr bool operator<=(CRField l, CRField r) { return static_cast<int>(l) <= static_cast<int>(r); }

enum class CRBit : uint8_t {
  kCr0Lt,
  kCr0Gt,
  kCr0Eq,
  kCr0So,
  kCr1Lt,
  kCr1Gt,
  kCr1Eq,
  kCr1So,
  kCr2Lt,
  kCr2Gt,
  kCr2Eq,
  kCr2So,
  kCr3Lt,
  kCr3Gt,
  kCr3Eq,
  kCr3So,
  kCr4Lt,
  kCr4Gt,
  kCr4Eq,
  kCr4So,
  kCr5Lt,
  kCr5Gt,
  kCr5Eq,
  kCr5So,
  kCr6Lt,
  kCr6Gt,
  kCr6Eq,
  kCr6So,
  kCr7Lt,
  kCr7Gt,
  kCr7Eq,
  kCr7So,

  // Overlaps with standard CR1 usage
  kCr1Fx = 4,
  kCr1Fex = 5,
  kCr1Vx = 6,
  kCr1Ox = 7,
};

constexpr bool operator>(CRBit l, CRBit r) { return static_cast<int>(l) > static_cast<int>(r); }
constexpr bool operator>=(CRBit l, CRBit r) { return static_cast<int>(l) >= static_cast<int>(r); }
constexpr bool operator<(CRBit l, CRBit r) { return static_cast<int>(l) < static_cast<int>(r); }
constexpr bool operator<=(CRBit l, CRBit r) { return static_cast<int>(l) <= static_cast<int>(r); }

constexpr CRBit gt_bit(CRField field) {
  return static_cast<CRBit>(static_cast<uint8_t>(field) * 4);
}
constexpr CRBit lt_bit(CRField field) {
  return static_cast<CRBit>(static_cast<uint8_t>(field) * 4 + 1);
}
constexpr CRBit eq_bit(CRField field) {
  return static_cast<CRBit>(static_cast<uint8_t>(field) * 4 + 2);
}
constexpr CRBit so_bit(CRField field) {
  return static_cast<CRBit>(static_cast<uint8_t>(field) * 4 + 3);
}

//////////////////////////////
// Predefined register sets //
//////////////////////////////
using GprSet = RegSet<GPR>;
using FprSet = RegSet<FPR>;
using CrSet = RegSet<CRBit>;

constexpr GprSet kAllGprs = GprSet(0xffffffff);
constexpr FprSet kAllFprs = FprSet(0xffffffff);
constexpr CrSet kAllCrs = CrSet(0xffffffff);

template <GPR... gprs>
constexpr GprSet gpr_mask_literal() {
  return GprSet{(0u | ... | (1 << static_cast<uint8_t>(gprs)))};
}
template <FPR... gprs>
constexpr FprSet fpr_mask_literal() {
  return FprSet{(0u | ... | (1 << static_cast<uint8_t>(gprs)))};
}
template <CRBit... gprs>
constexpr CrSet cr_mask_literal() {
  return CrSet{(0u | ... | (1 << static_cast<uint8_t>(gprs)))};
}
template <typename... Ts>
constexpr GprSet gpr_mask(Ts... args) {
  return GprSet{(0u | ... | static_cast<uint32_t>(1 << static_cast<uint8_t>(args)))};
}
template <typename... Ts>
constexpr FprSet fpr_mask(Ts... args) {
  return FprSet{(0u | ... | static_cast<uint32_t>(1 << static_cast<uint8_t>(args)))};
}
template <typename... Ts>
constexpr CrSet cr_mask(Ts... args) {
  return CrSet{(0u | ... | static_cast<uint32_t>(1 << static_cast<uint8_t>(args)))};
}

constexpr GprSet gpr_range(GPR start) {
  GprSet excl = gpr_mask(start)._set - 1;
  return kAllGprs - excl;
}
constexpr FprSet fpr_range(FPR start) {
  FprSet excl = fpr_mask(start)._set - 1;
  return kAllFprs - excl;
}

///////////////////////////////
// GPR and FPR operand types //
///////////////////////////////
enum class DataType : uint8_t {
  kS1,
  kS2,
  kS4,
  kSingle,
  kDouble,
  kPackedSingle,
  kSingleOrDouble,
  kUnknown,
  kLast = kUnknown,
};
static_assert(static_cast<uint8_t>(DataType::kLast) <= 0b111);

/////////////////////////////
// Read/Write source types //
/////////////////////////////
struct GPRSlice {
  GPR _reg : 5;
  DataType _type : 3;
  constexpr bool operator==(GPRSlice rhs) const { return _reg == rhs._reg && _type == rhs._type; }
};

struct FPRSlice {
  FPR _reg : 5;
  DataType _type : 3;
  constexpr bool operator==(FPRSlice rhs) const { return _reg == rhs._reg && _type == rhs._type; }
};

struct MemRegOff {
  GPR _base : 5;
  DataType _type : 3;
  int16_t _offset;
  constexpr bool operator==(MemRegOff rhs) const {
    return _base == rhs._base && _type == rhs._type && _offset == rhs._offset;
  }
};

struct MemRegReg {
  GPR _base : 5;
  DataType _type : 3;
  GPR _offset;
  constexpr bool operator==(MemRegReg rhs) const {
    return _base == rhs._base && _type == rhs._type && _offset == rhs._offset;
  }
};

struct MultiReg {
  GPR _low : 5;
  DataType _type : 3;
  constexpr bool operator==(MultiReg rhs) const { return _low == rhs._low && _type == rhs._type; }
};

struct SIMM {
  int16_t _imm_value;
  constexpr bool operator==(SIMM rhs) const { return _imm_value == rhs._imm_value; }
};

struct UIMM {
  uint16_t _imm_value;
  constexpr bool operator==(UIMM rhs) const { return _imm_value == rhs._imm_value; }
};

struct RelBranch {
  int32_t _rel_32;
  constexpr bool operator==(RelBranch rhs) const { return _rel_32 == rhs._rel_32; }
};

enum class SPR {
  kXer = 1,
  kLr = 8,
  kCtr = 9,
};

enum class TBR {
  kTbl = 268,
  kTbu = 269,
};

enum class XERBit {
  kCA,  // Carry
  kOV,  // Overflow
  kSO,  // Overflow summary
  kBC,  // 25-31
};

struct AuxImm {
  uint32_t _val;
  constexpr bool operator==(AuxImm rhs) const { return _val == rhs._val; }
};

enum class FPSCRBit : uint32_t {
  kNone = 0,
  kAll = 0xffffffff,
  kExceptionMask = 0b00000000111000000001111111111001,
  kWriteMask = 0b11111111111111111111111111111001,

  kFx = 1u << 0,
  kFex = 1u << 1,
  kVx = 1u << 2,
  kOx = 1u << 3,
  kUx = 1u << 4,
  kZx = 1u << 5,
  kXx = 1u << 6,
  kVxsnan = 1u << 7,
  kVxisi = 1u << 8,
  kVxidi = 1u << 9,
  kVxzdz = 1u << 10,
  kVximz = 1u << 11,
  kVxvc = 1u << 12,
  kFr = 1u << 13,
  kFi = 1u << 14,
  kFprf = 0b11111 << 15,
  kFpc = 1 << 15,
  kFpcc = 0b1111 << 16,
  kVxsoft = 1u << 21,
  kVxsqrt = 1u << 22,
  kVxcvi = 1u << 23,
  kVe = 1u << 24,
  kOe = 1u << 25,
  kUe = 1u << 26,
  kZe = 1u << 27,
  kXe = 1u << 28,
  kNi = 1u << 29,
  kRn = 0b11u << 30,
};
GEN_FLAG_OPERATORS(FPSCRBit)

enum class InstFlags : uint32_t {
  kNone = 0b000,
  kAll = 0b000,
  kAbsoluteAddr = 0b001,
  kPsLoadsOne = 0b010,
  kLongMode = 0b100,
};
GEN_FLAG_OPERATORS(InstFlags)

enum class InstSideFx : uint32_t {
  kNone = 0b000000,
  kAll = 0b111111,
  // Effectively executes `cmpwi cr0, rD, 0`
  //  - Arithmetic instructions with the `.` suffix
  kWritesRecord = 0b000001,
  // Writes the FP exception info to CR1
  //  - FP Arithmetic instructions with the `.` suffix
  kWritesFpRecord = 0b000010,
  // Checks if the arithmetic result overflowed, saving the result to XER[OV] and XER[SO]
  //  - Arithmetic instructions with the o suffix
  kWritesOVSO = 0b000100,
  // Effectively executes `add ra, ra, rb` or `addi ra, ra, d`
  //  - Updating load/store instructions (e.g. stwu, lwzu)
  kWritesBaseReg = 0b001000,
  // Writes the next PC to the LR
  //  - Branches with LK bit
  kWritesLR = 0b010000,
  // Checks if the arithmetic result carried, saving the result to XER[CA]
  kWritesCA = 0b100000,
};
GEN_FLAG_OPERATORS(InstSideFx)

using DataSource = std::variant<GPRSlice,
  FPRSlice,
  CRField,
  CRBit,
  MemRegOff,
  MemRegReg,
  MultiReg,
  SPR,
  TBR,
  FPSCRBit,
  SIMM,
  UIMM,
  RelBranch,
  AuxImm,
  XERBit>;

constexpr bool is_memory_ref(DataSource const& ds) {
  return std::holds_alternative<MemRegOff>(ds) || std::holds_alternative<MemRegReg>(ds);
}
}  // namespace decomp::ppc
