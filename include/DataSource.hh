#pragma once

#include <cstdint>
#include <variant>

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

enum class CRField {
  kCR0, kCR1, kCR2, kCR3,
  kCR4, kCR5, kCR6, kCR7,
};

enum class CRFlag {
  kLt, kGt, kEq, kSo,
};

union CRBit {
  struct {
    CRFlag _flag : 2;
    CRField _field : 3;
  } _parts;
  uint32_t _val;
};

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

struct FPSCR { uint8_t _dummy = 0; };

using DataSource =
  std::variant<GPR, FPR, CRField, CRFlag, CRBit, MemRegOff, MemRegReg, SPR, TBR, FPSCR>;
using ImmSource =
  std::variant<SIMM, UIMM, RelBranch, AuxImm>;
