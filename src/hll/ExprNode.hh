#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace decomp::hll {
enum class NodeType {
  kSequence,
  kDeclaration,
  kTypeConvert,
  kPrimitiveImm,
  kVariable,
  kAssignment,
  kBinaryOp,
  kUnaryOp,
  kIf,
  kWhile,
  kFor,
  kGoto,
  kReturn,
  kRoutineCall,
};

using NodeRef = uint32_t;
constexpr NodeRef kInvalidNodeRef = 0xffffffff;

struct ExprNode {
  NodeType _ntype;
  NodeRef _nid;
};

template <NodeType NT>
struct StaticTypedExprNode : ExprNode {
  inline static constexpr NodeType kStaticType = NT;
};

///////////
// Nodes //
///////////
enum class BinaryOpType {
  kAdd,
  kSub,
  kMul,
  kDiv,
  kAnd,
  kOr,
  kEq,
  kNeq,
  kGt,
  kLt,
  kGe,
  kLe,
  kLsh,
  kRsh,
  kBitAnd,
  kBitOr,
  kBitXor,
};

enum class UnaryOpType {
  kNeg,
  kNot,
  kBitNot,
  kDereference,
  kReference,
};

enum class PrimitiveType {
  kS1,
  kS2,
  kS4,
  kS8,
  kU1,
  kU2,
  kU4,
  kU8,
  kSingle,
  kDouble,
  kBoolean,
  kVoid,
};

constexpr std::string_view primitive_type_str(PrimitiveType type) {
  switch (type) {
    case PrimitiveType::kS1:
      return "int8_t";

    case PrimitiveType::kS2:
      return "int16_t";

    case PrimitiveType::kS4:
      return "int32_t";

    case PrimitiveType::kS8:
      return "int64_t";

    case PrimitiveType::kU1:
      return "uint8_t";

    case PrimitiveType::kU2:
      return "uint16_t";

    case PrimitiveType::kU4:
      return "uint32_t";

    case PrimitiveType::kU8:
      return "uint64_t";

    case PrimitiveType::kSingle:
      return "float";

    case PrimitiveType::kDouble:
      return "double";

    case PrimitiveType::kBoolean:
      return "bool";

    case PrimitiveType::kVoid:
      return "void";

    default:
      return "invalid";
  }
}

struct Sequence : StaticTypedExprNode<NodeType::kSequence> {
  std::vector<NodeRef> _seq;
};

struct Declaration : StaticTypedExprNode<NodeType::kDeclaration> {
  uint32_t _varid;
};

struct TypeConvert : StaticTypedExprNode<NodeType::kTypeConvert> {
  NodeRef _sub;
  PrimitiveType _convto;
};

// Leaves
struct PrimitiveImm : StaticTypedExprNode<NodeType::kPrimitiveImm> {
  uint64_t _val;
  PrimitiveType _ptype;

  template <typename T>
  void assign(T val) {
    _val = 0;
    *reinterpret_cast<T*>(&_val) = val;
  }
};

struct Variable : StaticTypedExprNode<NodeType::kVariable> {
  uint32_t _varid;
};

// Operations
struct Assignment : StaticTypedExprNode<NodeType::kAssignment> {
  NodeRef _assignee;
  NodeRef _val;
};

struct BinaryOp : StaticTypedExprNode<NodeType::kBinaryOp> {
  NodeRef _lhs;
  NodeRef _rhs;
  BinaryOpType _operation;
};

struct UnaryOp : StaticTypedExprNode<NodeType::kUnaryOp> {
  NodeRef _sub;
  UnaryOpType _operation;
};

// Flow control
struct If : StaticTypedExprNode<NodeType::kIf> {
  NodeRef _cond;
  NodeRef _true;
  NodeRef _false;
};

struct While : StaticTypedExprNode<NodeType::kWhile> {
  NodeRef _cond;
  NodeRef _body;
};

struct For : StaticTypedExprNode<NodeType::kFor> {
  NodeRef _init;
  NodeRef _cond;
  NodeRef _body;
  NodeRef _iter;
};

struct Goto : StaticTypedExprNode<NodeType::kGoto> {
  uint32_t _target_id;
};

struct Return : StaticTypedExprNode<NodeType::kReturn> {
  NodeRef _val;
};

struct RoutineCall : StaticTypedExprNode<NodeType::kRoutineCall> {
  std::vector<NodeRef> _param;
  uint32_t _target_va;
};
}  // namespace decomp::hll
