#pragma once

#include <variant>

#include "ppc/DataSource.hh"
#include "utl/ReservedVector.hh"

namespace decomp::ir {
// Binds: store each individual table (stack and reg tables)
// 32 iv trees for 32 regs, store an associated register and temp idx
// Problem: 32 registers each with unique bind regions, so you'd need 32 interval trees (bad idea??)
// maybe it's a good idea dude
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
};

struct TVRef {
  uint8_t _table : 1;
  uint32_t _idx : 31;
  RefType _reftype;
};

struct Immediate {
  bool _signed : 1;
  int32_t _val : 31;
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
