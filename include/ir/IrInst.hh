#pragma once

#include <variant>

#include "ppc/DataSource.hh"
#include "utl/ReservedVector.hh"

namespace decomp::ir {
enum class IrOpcode {
  // Data movement
  kMov,
  kStore,
  kLoad,

  // Comparison
  kCmp,
  kRcTest,

  // Indirection
  kCall,
  kReturn,

  // Bit
  kLsh,
  kRsh,
  kRol,
  kRor,
  kAndB,
  kOrB,
  kXorB,
  kNotB,

  // Arithmetic
  kAdd,
  kAddc,
  kSub,
  kMul,
  kDiv,
  kNeg,
  kSqrt,
  kAbs,

  // Intrinsic - no high level translation
  kIntrinsic,

  // Optimization barrier (sync/isync)
  kOptBarrier,
};

enum class RefType : uint8_t {
  kS1,
  kS2,
  kS4,
  kU1,
  kU2,
  kU4,
  kSingle,
  kDouble,
  kInvalid,
};

enum class TVTable : uint8_t {
  kGprTable,
  kFprTable,
  kCrTable,
  kSprTable,
};
struct TVRef {
  TVTable _table : 2;
  uint32_t _idx : 30;
  RefType _reftype;
};

struct MemRef {
  uint32_t _gpr_tv;
  int16_t _off;
};

struct StackRef {
  int16_t _off;
};

struct ParamRef {
  int16_t _off;
};

struct Immediate {
  uint32_t _val;
  bool _signed;
};

struct FunctionRef {
  uint32_t _func_va;
};

struct ConditionRef {
  uint32_t _bits;
};

using OpVar = std::variant<TVRef, MemRef, StackRef, ParamRef, Immediate, FunctionRef, ConditionRef>;

struct IrInst {
  IrInst(IrOpcode opc) : _opc(opc) {}
  IrInst(IrOpcode opc, OpVar v0) : _opc(opc), _ops({v0}) {}
  IrInst(IrOpcode opc, OpVar v0, OpVar v1) : _opc(opc), _ops({v0, v1}) {}
  IrInst(IrOpcode opc, OpVar v0, OpVar v1, OpVar v2) : _opc(opc), _ops({v0, v1, v2}) {}
  IrOpcode _opc;
  reserved_vector<OpVar, 3> _ops;
};
}  // namespace decomp::ir
