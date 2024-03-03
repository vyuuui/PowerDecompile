#include "hll/Structurizer.hh"

#include <fmt/format.h>
#include <limits>
#include <numeric>
#include <set>
#include <string>
#include <vector>

#include "hll/ExprNode.hh"

namespace decomp::hll {
namespace {
using namespace decomp::ir;

struct NodeAllocator {
  std::vector<ExprNode*> _node_list;
  std::vector<std::string> _var_list;

  template <typename T>
  T* alloc_node() {
    return reinterpret_cast<T*>(alloc_node_helper(T::kStaticType));
  }

  ExprNode* alloc_node_helper(NodeType nt) {
    NodeRef new_nid = _node_list.size(); 
    ExprNode* ret = nullptr;

    switch (nt) {
      case NodeType::kSequence:
        ret = new Sequence;
        break;

      case NodeType::kDeclaration:
        ret = new Declaration;
        break;

      case NodeType::kTypeConvert:
        ret = new TypeConvert;
        break;

      case NodeType::kPrimitiveImm:
        ret = new PrimitiveImm;
        break;

      case NodeType::kVariable:
        ret = new Variable;
        break;

      case NodeType::kAssignment:
        ret = new Assignment;
        break;

      case NodeType::kBinaryOp:
        ret = new BinaryOp;
        break;

      case NodeType::kUnaryOp:
        ret = new UnaryOp;
        break;

      case NodeType::kIf:
        ret = new If;
        break;

      case NodeType::kWhile:
        ret = new While;
        break;

      case NodeType::kFor:
        ret = new For;
        break;

      case NodeType::kGoto:
        ret = new Goto;
        break;

      case NodeType::kReturn:
        ret = new Return;
        break;

      case NodeType::kRoutineCall:
        ret = new RoutineCall;
        break;
    }

    _node_list.push_back(ret);

    ret->_nid = new_nid;
    ret->_ntype = nt;
    return ret;
  }

  uint32_t create_var(std::string const& name) {
    uint32_t id = _var_list.size();
    _var_list.emplace_back(name);
    return id;
  }
};

ExprNode* convert_inst(OpVar var, NodeAllocator& na)
{
  ExprNode* p_var = nullptr;
  if (auto* tv = std::get_if<TempVar>(&var); tv != nullptr) {
    uint32_t var_id = na.create_var(fmt::format("t{}", tv->_base._idx));
    Variable* v = na.alloc_node<Variable>();
    v->_varid = var_id;
    p_var = v;
  } else if (auto* mr = std::get_if<MemRef>(&var); mr != nullptr) {

  } else if (auto* sr = std::get_if<StackRef>(&var); sr != nullptr) {
    uint32_t var_id = na.create_var(fmt::format("stk_{:x}", sr->_off));
    Variable* v = na.alloc_node<Variable>();
    v->_varid = var_id;

    if (sr->_addrof) {
      // UnaryOp {kReference subnode -> Variable{stk_whatever}}
      UnaryOp* op = na.alloc_node<UnaryOp>();
      op->_sub = v->_nid;
      p_var = op;
    } else {
      p_var = v;
    }
  } else if (auto* pr = std::get_if<ParamRef>(&var); pr != nullptr) {
    uint32_t var_id = na.create_var(fmt::format("param_{}", pr->));
    Variable* v = na.alloc_node<Variable>();
    v->_varid = var_id;
    p_var = v;
  } else if (auto* imm = std::get_if<Immediate>(&var); imm != nullptr) {
    PrimitiveImm* p = na.alloc_node<PrimitiveImm>();
    if (imm->_signed) {
      p->assign<int32_t>(static_cast<int32_t>(imm->_val));
    } else {
      p->assign<uint32_t>(imm->_val);
    }
    p_var = p;
  } else if (auto* fr = std::get_if<FunctionRef>(&var); fr != nullptr) {
    assert(false);
  } else {
    assert(false);
  }

  return p_var;
}

ExprNode* untile_single(IrInst const& inst, NodeAllocator& na) {
  ExprNode* p_expr = nullptr;
  switch(inst._opc)
  {
  case IrOpcode::kMov:
    p_expr = new Assignment();
    break;

  case IrOpcode::kStore:
  break;
  
  case IrOpcode::kLsh:
  break;

  case IrOpcode::kRsh:
  break;

  case IrOpcode::kRol:
  break;

  case IrOpcode::kRor:
  break;

  case IrOpcode::kAndB:
  break;

  case IrOpcode::kOrB:
  break;

  case IrOpcode::kXorB:
  break;

  case IrOpcode::kNotB:
  break;

  }
  return p_expr;
}
}  // namespace

void untile_block(ir::IrBlock const& blk) {
  NodeAllocator na;

  std::vector<ExprNode*> unmerged_ast;
  for (auto const& inst : blk._insts) {
    unmerged_ast.push_back(untile_single(inst, na));
  }
}
}  // namespace decomp::hll
