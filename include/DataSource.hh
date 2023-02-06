#pragma once

#include <cstdint>
#include <variant>

#include "FlagsEnum.hh"

enum class GPR {
  kR0, kR1, kR2, kR3,
  kR4, kR5, kR6, kR7,
  kR8, kR9, kR10, kR11,
  kR12, kR13, kR14, kR15,
  kR16, kR17, kR18, kR19,
  kR20, kR21, kR22, kR23,
  kR24, kR25, kR26, kR27,
  kR28, kR29, kR30, kR31,
};

enum class FPR {
  kF0, kF1, kF2, kF3,
  kF4, kF5, kF6, kF7,
  kF8, kF9, kF10, kF11,
  kF12, kF13, kF14, kF15,
  kF16, kF17, kF18, kF19,
  kF20, kF21, kF22, kF23,
  kF24, kF25, kF26, kF27,
  kF28, kF29, kF30, kF31,
};

enum class CRBit : uint32_t {
  kNone = 0,
  kAll  = 0xffffffff,

  kCr0  = 0b1111u << 0,
  kCr1  = 0b1111u << 4,
  kCr2  = 0b1111u << 8,
  kCr3  = 0b1111u << 12,
  kCr4  = 0b1111u << 16,
  kCr5  = 0b1111u << 20,
  kCr6  = 0b1111u << 24,
  kCr7  = 0b1111u << 28,

  kLt   = 1u << 0,
  kGt   = 1u << 1,
  kEq   = 1u << 2,
  kSo   = 1u << 3,

  kFx   = 1u << 4,
  kFex  = 1u << 5,
  kVx   = 1u << 6,
  kOx   = 1u << 7,
};
GEN_FLAG_OPERATORS(CRBit)

struct MemRegOff {
  GPR _base;
  int32_t _offset;
};

struct MemRegReg {
  GPR _base;
  GPR _offset;
};

struct SIMM {
  int32_t _imm_value;
};

struct UIMM {
  uint32_t _imm_value;
};

struct RelBranch {
  int32_t _rel_32;
};

enum class SPR {
  kXer = 1, kLr = 8, kCtr = 9,
};

enum class TBR {
  kTbl = 268, kTbu = 269,
};

struct AuxImm {
  uint32_t _val;
};

enum class FPSCRBit : uint32_t {
  kNone          = 0,
  kAll           = 0xffffffff,
  kExceptionMask = 0b00000000111000000001111111111001,
  kWriteMask     = 0b11111111111111111111111111111001,

  kFx            = 1u << 0,
  kFex           = 1u << 1,
  kVx            = 1u << 2,
  kOx            = 1u << 3,
  kUx            = 1u << 4,
  kZx            = 1u << 5,
  kXx            = 1u << 6,
  kVxsnan        = 1u << 7,
  kVxisi         = 1u << 8,
  kVxidi         = 1u << 9,
  kVxzdz         = 1u << 10,
  kVximz         = 1u << 11,
  kVxvc          = 1u << 12,
  kFr            = 1u << 13,
  kFi            = 1u << 14,
  kFprf          = 0b11111 << 15,
  kFpc           = 1 << 15,
  kFpcc          = 0b1111 << 16,
  kVxsoft        = 1u << 21,
  kVxsqrt        = 1u << 22,
  kVxcvi         = 1u << 23,
  kVe            = 1u << 24,
  kOe            = 1u << 25,
  kUe            = 1u << 26,
  kZe            = 1u << 27,
  kXe            = 1u << 28,
  kNi            = 1u << 29,
  kRn            = 0b11u << 30,
};
GEN_FLAG_OPERATORS(FPSCRBit)

using DataSource =
  std::variant<GPR, FPR, CRBit, MemRegOff, MemRegReg, SPR, TBR, FPSCRBit>;
using ImmSource =
  std::variant<SIMM, UIMM, RelBranch, AuxImm>;
