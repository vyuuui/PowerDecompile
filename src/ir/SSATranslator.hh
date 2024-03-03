#pragma once

#include <array>
#include <optional>
#include <variant>
#include <vector>
#include "ppc/SubroutineGraph.hh"
#include "ppc/Subroutine.hh"
#include "utl/ReservedVector.hh"
#include "utl/FlowGraph.hh"

namespace decomp::ir {
enum class SSAOpcode {
  // Data movement
  kMov,

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

enum class TempClass : uint8_t {
  kGpr,
  kFpr,
  kCondition,
  kStack,
  kCarry,
  kTempIntegral,
  kTempFloating,
  kTempCondition,
};

union VarSrc {
  ppc::GPR _gpr;
  ppc::FPR _fpr;
  struct {
    ppc::CRField _fld;
    uint8_t _bits;
  } _cr;
  // XER[CA] implied by TempClass::kCarry
  uint16_t _stkoff;
};

constexpr uint32_t kInvalidTmp = 0xffffffff;
struct SSAVar {
  TempClass _class;
  VarSrc _src;
  uint32_t _idx;
};

struct MemRef {
  SSAVar _base;
  int16_t _off;
};

struct Immediate {
  uint32_t _val;
  bool _signed;
};

enum class DataSize {
  kS8,
  kS16,
  kS32,
  kS64,
};

struct SSAOperand {
  std::variant<std::monostate, ppc::ReadSource, ppc::WriteSource> _ppc_src;
  std::variant<std::monostate, SSAVar, MemRef, Immediate> _operand;
  DataSize _sz;

  SSAOperand(ppc::ReadSource read, DataSize sz) : _ppc_src(read), _operand(std::monostate{}), _sz(sz) {}
  SSAOperand(ppc::WriteSource write, DataSize sz) : _ppc_src(write), _operand(std::monostate{}), _sz(sz) {}
  SSAOperand(Immediate imm, DataSize sz) : _ppc_src(std::monostate{}), _operand(imm), _sz(sz) {}
  SSAOperand(Immediate imm, DataSize sz) : _ppc_src(std::monostate{}), _operand(imm), _sz(sz) {}
  SSAOperand(SSAOperand const& rhs) : _ppc_src(rhs._ppc_src), _operand(rhs._operand), _sz(rhs._sz) {}
  static SSAOperand mktemp_int(DataSize sz) {
    return SSAOperand {
      ._operand = SSAVar {
        ._class = TempClass::kTempIntegral,
        ._idx = kInvalidTmp,
      },
      ._sz = sz,
    };
  }
  static SSAOperand mktemp_flt(DataSize sz) {
    return SSAOperand {
      ._operand = SSAVar {
        ._class = TempClass::kTempFloating,
        ._idx = kInvalidTmp,
      },
      ._sz = sz,
    };
  }
  static SSAOperand mktemp_cond(DataSize sz) {
    return SSAOperand {
      ._operand = SSAVar {
        ._class = TempClass::kTempCondition,
        ._idx = kInvalidTmp,
      },
      ._sz = sz,
    };
  }
};

enum class SignednessHint {
  kNoHint,
  kSigned,
  kUnsigned,
};

struct SSAInst {
  SSAInst(SSAOpcode opc, SignednessHint hint = SignednessHint::kNoHint) : _opc(opc), _hint(hint) {}
  SSAInst(SSAOpcode opc, SSAOperand v0, SignednessHint hint = SignednessHint::kNoHint) : _opc(opc), _ops({v0}), _hint(hint) {}
  SSAInst(SSAOpcode opc, SSAOperand v0, SSAOperand v1, SignednessHint hint = SignednessHint::kNoHint) : _opc(opc), _ops({v0, v1}), _hint(hint) {}
  SSAInst(SSAOpcode opc, SSAOperand v0, SSAOperand v1, SSAOperand v2, SignednessHint hint = SignednessHint::kNoHint) : _opc(opc), _ops({v0, v1, v2}), _hint(hint) {}
  SSAOpcode _opc;
  reserved_vector<SSAOperand, 3> _ops;
  SignednessHint _hint;
};

struct SSABlock {
  std::vector<SSAInst> _insts;
  // Condition check info
  std::optional<SSAVar> _cond;
};
using SSABlockVertex = FlowVertex<SSABlock>;

struct SSARoutine {
  FlowGraph<SSABlock> _graph;
  std::array<uint32_t, 8> _int_param;
  std::array<uint32_t, 13> _flt_param;
  std::vector<uint16_t> _stk_params;
  uint8_t _num_int_param;
  uint8_t _num_flt_param;

  SSARoutine()
      : _num_int_param(0),
        _num_flt_param(0) {
    std::fill(_int_param.begin(), _int_param.end(), kInvalidTmp);
    std::fill(_flt_param.begin(), _flt_param.end(), kInvalidTmp);
  }
};

SSARoutine translate_to_ssa(ppc::Subroutine const& routine);
}