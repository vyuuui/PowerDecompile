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

enum class IrType : uint8_t {
  kS1,
  kS2,
  kS4,
  kU1,
  kU2,
  kU4,
  kSingle,
  kDouble,
  kBoolean,
  kInvalid,
};

enum class TempClass : uint8_t {
  kIntegral,
  kFloating,
  kCondition,
};

struct TVRef {
  uint32_t _idx;
  TempClass _class;
  IrType _reftype;
};

struct ConditionTVRef {
  uint32_t _idx;
  TempClass _class;
  IrType _reftype;
  uint8_t _condbits;
};

union TempVar {
  TVRef _base;
  ConditionTVRef _cnd;

  static TempVar integral(uint32_t index, IrType reftype) {
    return TempVar{._base = TVRef{
                     ._idx = index,
                     ._class = TempClass::kIntegral,
                     ._reftype = reftype,
                   }};
  }

  static TempVar floating(uint32_t index, IrType reftype) {
    return TempVar{._base = TVRef{
                     ._idx = index,
                     ._class = TempClass::kFloating,
                     ._reftype = reftype,
                   }};
  }

  static TempVar condition(uint32_t index, uint8_t condbits) {
    return TempVar{._cnd = ConditionTVRef{
                     ._idx = index,
                     ._class = TempClass::kCondition,
                     ._reftype = IrType::kBoolean,
                     ._condbits = condbits,
                   }};
  }
};

struct MemRef {
  uint32_t _gpr_tv;
  int16_t _off;
};

struct StackRef {
  int16_t _off;
  bool _addrof;
};

struct ParamRef {
  uint32_t _param_idx;
  bool _addrof;
};

struct Immediate {
  uint32_t _val;
  bool _signed;
};

struct FunctionRef {
  uint32_t _func_va;
};

using OpVar = std::variant<TempVar, MemRef, StackRef, ParamRef, Immediate, FunctionRef>;

struct IrInst {
  IrInst(IrOpcode opc) : _opc(opc) {}
  IrInst(IrOpcode opc, OpVar v0) : _opc(opc), _ops({v0}) {}
  IrInst(IrOpcode opc, OpVar v0, OpVar v1) : _opc(opc), _ops({v0, v1}) {}
  IrInst(IrOpcode opc, OpVar v0, OpVar v1, OpVar v2) : _opc(opc), _ops({v0, v1, v2}) {}
  IrOpcode _opc;
  reserved_vector<OpVar, 3> _ops;
};
}  // namespace decomp::ir
