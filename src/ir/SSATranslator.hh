#pragma once

#include <array>
#include <optional>
#include <variant>
#include <vector>

#include "ppc/Subroutine.hh"
#include "ppc/SubroutineGraph.hh"
#include "utl/FlowGraph.hh"
#include "utl/ReservedVector.hh"

namespace decomp::ir {
enum class SSABinaryExpr {
  // Bit
  kLsh,
  kRsh,
  kRol,
  kRor,
  kAndB,
  kOrB,
  kXorB,

  // Arithmetic
  kAdd,
  kSub,
  kMul,
  kDiv,

  kCmpGrtr,
  kCmpLess,
  kCmpEq,
};

enum class SSAUnaryExpr {
  kNotB,
  kNeg,
  kSqrt,
  kAbs,
  // High half of operand
  // Inferred type = sizeof(sub) / 2, inherits signedness
  kHigh,
  kSizeCast,
};

enum class SSAExprType {
  kUnary,
  kBinary,
  kMem,
  kIntImm,
  kFltImm,
  kTempVar,
  kIntrinsic,
};

enum class TempClass : uint8_t {
  kGpr,
  kFpr,
  kCondition,
  kStack,
  kSpecialReg,
  kTempIntegral,
  kTempFloating,
  kTempCondition,
};

enum class DataSize {
  kSInfer,
  kS8,
  kS16,
  kS32,
  kS64,
  // Type for single CR bits
  kS1,
};

constexpr DataSize dt2ds(ppc::DataType dt) {
  switch (dt) {
    case ppc::DataType::kS1:
      return DataSize::kS8;

    case ppc::DataType::kS2:
      return DataSize::kS16;

    case ppc::DataType::kS4:
    case ppc::DataType::kSingle:
      return DataSize::kS32;

    case ppc::DataType::kDouble:
    case ppc::DataType::kPackedSingle:
      return DataSize::kS64;

    default:
      return DataSize::kSInfer;
  }
}

enum class SignednessHint {
  kNoHint,
  kSigned,
  kUnsigned,
};

struct SSAExpr {
  SSAExpr(SSAExprType type, DataSize sz = DataSize::kSInfer, SignednessHint hint = SignednessHint::kNoHint)
      : type(type), sz(sz), hint(hint) {}

  SSAExprType type;
  DataSize sz;
  SignednessHint hint;
};

struct MemExpr : SSAExpr {
  MemExpr(SSAExpr* addr, DataSize sz = DataSize::kSInfer, SignednessHint hint = SignednessHint::kNoHint)
      : SSAExpr(SSAExprType::kMem, sz, hint), addr(addr) {}
  MemExpr(ppc::DataSource src, SignednessHint hint = SignednessHint::kNoHint);

  SSAExpr* addr;
};

struct BinaryExpr : SSAExpr {
  BinaryExpr(SSABinaryExpr op,
    SSAExpr* left,
    SSAExpr* right,
    DataSize sz = DataSize::kSInfer,
    SignednessHint hint = SignednessHint::kNoHint)
      : SSAExpr(SSAExprType::kBinary, sz, hint), op(op), left(left), right(right) {}

  SSABinaryExpr op;
  SSAExpr* left;
  SSAExpr* right;
};

struct UnaryExpr : SSAExpr {
  UnaryExpr(
    SSAUnaryExpr op, SSAExpr* sub, DataSize sz = DataSize::kSInfer, SignednessHint hint = SignednessHint::kNoHint)
      : SSAExpr(SSAExprType::kUnary, sz, hint), op(op), sub(sub) {}

  SSAUnaryExpr op;
  SSAExpr* sub;
};

struct IntegerImm : SSAExpr {
  IntegerImm(uint32_t val, DataSize sz = DataSize::kSInfer, SignednessHint hint = SignednessHint::kNoHint)
      : SSAExpr(SSAExprType::kIntImm, sz, hint), val(val) {}

  uint32_t val;
};

struct FloatingImm : SSAExpr {
  FloatingImm(float val)
    : SSAExpr(SSAExprType::kFltImm, DataSize::kS32, SignednessHint::kSigned) {}

  float val;
};

struct IntrinsicExpr : SSAExpr {
  IntrinsicExpr(std::string_view intrin_name,
    SSAExpr* src1,
    SSAExpr* src2,
    DataSize sz = DataSize::kSInfer,
    SignednessHint hint = SignednessHint::kNoHint)
      : SSAExpr(SSAExprType::kIntrinsic, sz, hint), intrin_name(intrin_name), src1(src1), src2(src2) {}

  std::string_view intrin_name;
  SSAExpr* src1;
  SSAExpr* src2;
};

union VarSrc {
  ppc::GPR _gpr;
  ppc::FPR _fpr;
  ppc::CRBit _crb;
  uint16_t _stkoff;
  std::string_view _sprg_name;
};

struct SSATempVar : SSAExpr {
  SSATempVar(TempClass tc, VarSrc src, uint32_t index, DataSize sz, SignednessHint hint = SignednessHint::kNoHint)
      : SSAExpr(SSAExprType::kTempVar, sz, hint), tc(tc), src(src), index(index) {}
  SSATempVar(ppc::GPRSlice gpr, SignednessHint hint = SignednessHint::kNoHint)
      : SSATempVar(TempClass::kGpr, VarSrc{._gpr = gpr._reg}, -1, dt2ds(gpr._type), hint) {}
  SSATempVar(ppc::FPRSlice fpr, SignednessHint hint = SignednessHint::kNoHint)
      : SSATempVar(TempClass::kGpr, VarSrc{._fpr = fpr._reg}, -1, dt2ds(fpr._type), hint) {}
  SSATempVar(ppc::CRBit crb)
      : SSATempVar(TempClass::kCondition, VarSrc{._crb = crb}, -1, DataSize::kS1, SignednessHint::kUnsigned) {}
  SSATempVar(std::string_view name, DataSize sz, SignednessHint hint = SignednessHint::kNoHint)
      : SSATempVar(TempClass::kSpecialReg, VarSrc{._sprg_name = name}, -1, sz, hint) {}

  static SSATempVar* from_datasource(ppc::DataSource src, SignednessHint hint = SignednessHint::kNoHint);

  TempClass tc;
  VarSrc src;
  uint32_t index;
};

enum class SSAStatementType {
  kAssignment,
  kCall,
  kIntrinsic,
  kJmp,
};

struct SSAStatement {
  SSAStatementType type;
  SSAStatement(SSAStatementType type) : type(type) {}
};

struct CallStatement : SSAStatement {
  CallStatement(SSAExpr* function) : SSAStatement(SSAStatementType::kCall), function(function) {}

  SSAExpr* function;
  std::vector<SSAExpr*> parameters;
};

struct JumpStatement : SSAStatement {
  JumpStatement(SSAExpr* loc) : SSAStatement(SSAStatementType::kJmp), loc(loc) {}

  SSAExpr* loc;
};

struct AssignmentStatement : SSAStatement {
  AssignmentStatement(MemExpr dst, SSAExpr* expr)
      : SSAStatement(SSAStatementType::kAssignment), store(dst), expr(expr) {}
  AssignmentStatement(SSATempVar dst, SSAExpr* expr)
      : SSAStatement(SSAStatementType::kAssignment), store(dst), expr(expr) {}

  std::variant<MemExpr, SSATempVar> store;
  SSAExpr* expr;
};

constexpr uint32_t kInvalidTmp = 0xffffffff;

struct SSABlock {
  std::vector<SSAStatement*> _insts;
  // Condition check info
  std::optional<SSAExpr*> _cond;
};
using SSABlockVertex = FlowVertex<SSABlock>;

struct SSARoutine {
  FlowGraph<SSABlock> _graph;
  std::array<uint32_t, 8> _int_param;
  std::array<uint32_t, 13> _flt_param;
  std::vector<uint16_t> _stk_params;
  uint8_t _num_int_param;
  uint8_t _num_flt_param;

  SSARoutine() : _num_int_param(0), _num_flt_param(0) {
    std::fill(_int_param.begin(), _int_param.end(), kInvalidTmp);
    std::fill(_flt_param.begin(), _flt_param.end(), kInvalidTmp);
  }
};

SSARoutine translate_to_ssa(ppc::Subroutine const& routine);
}  // namespace decomp::ir
