#pragma once

#include <variant>

#include "ppc/DataSource.hh"
#include "utl/ReservedVector.hh"

namespace decomp::ir {
struct RegBind {
  uint32_t _va_lo;
  uint32_t _va_hi;
  std::variant<ppc::GPR, ppc::FPR> _reg;
};
struct StackBind {};
using TempBind = std::variant<RegBind, StackBind>;

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

struct Immediate {
  uint32_t _val;
  bool _signed;
};

using OpVar = std::variant<TVRef, Immediate>;

struct IrInst {
  IrInst(IrOpcode opc) : _opc(opc) {}
  IrInst(IrOpcode opc, OpVar v0) : _opc(opc), _ops({v0}) {}
  IrInst(IrOpcode opc, OpVar v0, OpVar v1) : _opc(opc), _ops({v0, v1}) {}
  IrInst(IrOpcode opc, OpVar v0, OpVar v1, OpVar v2) : _opc(opc), _ops({v0, v1, v2}) {}
  IrOpcode _opc;
  reserved_vector<OpVar, 3> _ops;
};
}  // namespace decomp::ir
