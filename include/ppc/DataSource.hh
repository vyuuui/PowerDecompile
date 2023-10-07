#pragma once

#include <cstdint>
#include <variant>

#include "utl/FlagsEnum.hh"

namespace decomp {
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

enum class CRBit : uint32_t {
  kNone = 0,
  kAll = 0xffffffff,

  kCr0 = 0b1111u << 0,
  kCr1 = 0b1111u << 4,
  kCr2 = 0b1111u << 8,
  kCr3 = 0b1111u << 12,
  kCr4 = 0b1111u << 16,
  kCr5 = 0b1111u << 20,
  kCr6 = 0b1111u << 24,
  kCr7 = 0b1111u << 28,

  kLt = 1u << 0,
  kGt = 1u << 1,
  kEq = 1u << 2,
  kSo = 1u << 3,

  kFx = 1u << 4,
  kFex = 1u << 5,
  kVx = 1u << 6,
  kOx = 1u << 7,
};
GEN_FLAG_OPERATORS(CRBit)

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

struct GPRSlice {
  GPR _reg : 5;
  DataType _type : 3;
};

struct FPRSlice {
  FPR _reg : 5;
  DataType _type : 3;
};

struct MemRegOff {
  GPR _base : 5;
  DataType _type : 3;
  int16_t _offset;
};

struct MemRegReg {
  GPR _base : 5;
  DataType _type : 3;
  GPR _offset;
};

struct SIMM {
  int16_t _imm_value;
};

struct UIMM {
  uint16_t _imm_value;
};

struct RelBranch {
  int32_t _rel_32;
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

struct AuxImm {
  uint32_t _val;
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
  kNone = 0b000000,
  kAll = 0b111111,
  kWritesRecord = 0b000001,
  kWritesXER = 0b000010,
  kWritesLR = 0b000100,
  kAbsoluteAddr = 0b001000,
  kPsLoadsOne = 0b010000,
  kLongMode = 0b100000,
};
GEN_FLAG_OPERATORS(InstFlags)

using DataSource = std::variant<GPRSlice, FPRSlice, CRBit, MemRegOff, MemRegReg, SPR, TBR, FPSCRBit>;
using ImmSource = std::variant<SIMM, UIMM, RelBranch, AuxImm>;

constexpr bool is_memory_ref(DataSource const& ds) {
  return std::holds_alternative<MemRegOff>(ds) || std::holds_alternative<MemRegReg>(ds);
}
}  // namespace decomp
