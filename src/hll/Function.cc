#include "hll/Function.hh"

#include <fmt/format.h>

#include <iomanip>
#include <sstream>

namespace decomp::hll {
namespace {
PrimitiveType ir_type_to_primitive_type(ir::IrType type) {
  switch (type) {
    case ir::IrType::kS1:
      return PrimitiveType::kS1;
    case ir::IrType::kS2:
      return PrimitiveType::kS1;
    case ir::IrType::kS4:
      return PrimitiveType::kS1;
    case ir::IrType::kU1:
      return PrimitiveType::kU1;
    case ir::IrType::kU2:
      return PrimitiveType::kU2;
    case ir::IrType::kU4:
      return PrimitiveType::kU4;
    case ir::IrType::kSingle:
      return PrimitiveType::kSingle;
    case ir::IrType::kDouble:
      return PrimitiveType::kDouble;
    case ir::IrType::kBoolean:
      return PrimitiveType::kBoolean;
    default:
      assert(false);
      break;
  }
}
}

class FormatPrinter {
  std::ostream& _sink;
  const int _tabsz;
  int _tablv;
  bool _needs_tab;

public:
  FormatPrinter(std::ostream& sink, int tabsz) : _sink(sink), _tabsz(tabsz), _tablv(0), _needs_tab(false) {}
  template <typename T>
  void write(T&& v) {
    if (_needs_tab) {
      _sink << std::setw(_tablv) << "";
      _needs_tab = false;
    }
    _sink << std::forward<T>(v);
  }

  void linebreak() {
    _sink << "\n";
    _needs_tab = true;
  }

  void indent() { _tablv += _tabsz; }

  void unindent() { _tablv = std::max(_tablv - _tabsz, 0); }
};

void Function::write_node(NodeRef node, FormatPrinter& printer) { write_node(lookup(node), printer); }

void Function::write_node(ExprNode* node, FormatPrinter& printer) {
  if (_node_has_lbl[node->_nid]) {
    // TODO: make this work actually correctly by inserting label line above
    printer.linebreak();
    printer.write(fmt::format("label_{}:", node->_nid));
    printer.linebreak();
  }

  switch (node->_ntype) {
    case NodeType::kSequence:
      write_sequence(static_cast<Sequence*>(node), printer);
      break;

    case NodeType::kDeclaration:
      write_declaration(static_cast<Declaration*>(node), printer);
      break;

    case NodeType::kTypeConvert:
      write_typeconvert(static_cast<TypeConvert*>(node), printer);
      break;

    case NodeType::kPrimitiveImm:
      write_primitiveimm(static_cast<PrimitiveImm*>(node), printer);
      break;

    case NodeType::kVariable:
      write_variable(static_cast<Variable*>(node), printer);
      break;

    case NodeType::kAssignment:
      write_assignment(static_cast<Assignment*>(node), printer);
      break;

    case NodeType::kBinaryOp:
      write_binaryop(static_cast<BinaryOp*>(node), printer);
      break;

    case NodeType::kUnaryOp:
      write_unaryop(static_cast<UnaryOp*>(node), printer);
      break;

    case NodeType::kIf:
      write_if(static_cast<If*>(node), printer);
      break;

    case NodeType::kWhile:
      write_while(static_cast<While*>(node), printer);
      break;

    case NodeType::kFor:
      write_for(static_cast<For*>(node), printer);
      break;

    case NodeType::kGoto:
      write_goto(static_cast<Goto*>(node), printer);
      break;

    case NodeType::kReturn:
      write_return(static_cast<Return*>(node), printer);
      break;

    case NodeType::kRoutineCall:
      write_routinecall(static_cast<RoutineCall*>(node), printer);
      break;

    default:
      break;
  }
}

void Function::write_sequence(Sequence* node, FormatPrinter& printer) {
  for (size_t i = 0; i < node->_seq.size(); i++) {
    ExprNode* n = lookup(node->_seq[i]);
    write_node(n, printer);
    // These nodes particularly should not have a semicolon appended
    switch (n->_ntype) {
      case NodeType::kIf:
      case NodeType::kWhile:
      case NodeType::kFor:
      case NodeType::kSequence:
        break;

      default:
        printer.write(';');
        break;
    }
    // Let the parent node handle the next linebreak (if, while, for, root)
    if (i < node->_seq.size() - 1) {
      printer.linebreak();
    }
  }
}

void Function::write_declaration(Declaration* node, FormatPrinter& printer) {
  printer.write(fmt::format("{} {}", primitive_type_str(_varmap[node->_varid].first), _varmap[node->_varid].second));
}

void Function::write_typeconvert(TypeConvert* node, FormatPrinter& printer) {
  printer.write(fmt::format("({})", primitive_type_str(node->_convto)));
  write_node(node->_sub, printer);
}

void Function::write_primitiveimm(PrimitiveImm* node, FormatPrinter& printer) {
  switch (node->_ptype) {
    case PrimitiveType::kS1:
      printer.write(static_cast<int8_t>(node->_val));
      break;

    case PrimitiveType::kS2:
      printer.write(static_cast<int16_t>(node->_val));
      break;

    case PrimitiveType::kS4:
      printer.write(static_cast<int32_t>(node->_val));
      break;

    case PrimitiveType::kS8:
      printer.write(static_cast<int64_t>(node->_val));
      break;

    case PrimitiveType::kU1:
      printer.write(static_cast<uint8_t>(node->_val));
      break;

    case PrimitiveType::kU2:
      printer.write(static_cast<uint16_t>(node->_val));
      break;

    case PrimitiveType::kU4:
      printer.write(static_cast<uint32_t>(node->_val));
      break;

    case PrimitiveType::kU8:
      printer.write(static_cast<uint64_t>(node->_val));
      break;

    case PrimitiveType::kSingle:
      printer.write(*reinterpret_cast<float const*>(&node->_val));
      printer.write('f');
      break;

    case PrimitiveType::kDouble:
      printer.write(*reinterpret_cast<double const*>(&node->_val));
      break;

    case PrimitiveType::kBoolean:
      printer.write(*reinterpret_cast<bool const*>(&node->_val));
      break;

    case PrimitiveType::kVoid:
      break;
  }
}

void Function::write_variable(Variable* node, FormatPrinter& printer) {
  printer.write(_varmap[node->_varid].second);
}

void Function::write_assignment(Assignment* node, FormatPrinter& printer) {
  write_node(node->_assignee, printer);
  printer.write(" = ");
  write_node(node->_val, printer);
}

void Function::write_binaryop(BinaryOp* node, FormatPrinter& printer) {
  printer.write("(");
  write_node(node->_lhs, printer);

  switch (node->_operation) {
    case BinaryOpType::kAdd:
      printer.write(" + ");
      break;

    case BinaryOpType::kSub:
      printer.write(" - ");
      break;

    case BinaryOpType::kMul:
      printer.write(" * ");
      break;

    case BinaryOpType::kDiv:
      printer.write(" / ");
      break;

    case BinaryOpType::kAnd:
      printer.write(" && ");
      break;

    case BinaryOpType::kOr:
      printer.write(" || ");
      break;

    case BinaryOpType::kEq:
      printer.write(" == ");
      break;

    case BinaryOpType::kNeq:
      printer.write(" != ");
      break;

    case BinaryOpType::kGt:
      printer.write(" > ");
      break;

    case BinaryOpType::kLt:
      printer.write(" < ");
      break;

    case BinaryOpType::kGe:
      printer.write(" >= ");
      break;

    case BinaryOpType::kLe:
      printer.write(" <= ");
      break;

    case BinaryOpType::kLsh:
      printer.write(" << ");
      break;

    case BinaryOpType::kRsh:
      printer.write(" >> ");
      break;

    case BinaryOpType::kBitAnd:
      printer.write(" & ");
      break;

    case BinaryOpType::kBitOr:
      printer.write(" | ");
      break;

    case BinaryOpType::kBitXor:
      printer.write(" ^ ");
      break;
  }

  write_node(node->_rhs, printer);
  printer.write(")");
}

void Function::write_unaryop(UnaryOp* node, FormatPrinter& printer) {
  switch (node->_operation) {
    case UnaryOpType::kNeg:
      printer.write('-');
      break;

    case UnaryOpType::kNot:
      printer.write('!');
      break;

    case UnaryOpType::kBitNot:
      printer.write('~');
      break;

    case UnaryOpType::kDereference:
      printer.write('*');
      break;
  }

  write_node(node->_sub, printer);
}

void Function::write_if(If* node, FormatPrinter& printer) {
  printer.write("if (");
  write_node(node->_cond, printer);
  printer.write(") {");
  printer.linebreak();
  printer.indent();
  write_node(node->_true, printer);
  printer.linebreak();
  printer.unindent();
  if (node->_false != kInvalidNodeRef) {
    printer.write("} else {");
    printer.linebreak();
    printer.indent();
    write_node(node->_false, printer);
    printer.linebreak();
    printer.unindent();
  }
  printer.write('}');
}

void Function::write_while(While* node, FormatPrinter& printer) {
  printer.write("while (");
  write_node(node->_cond, printer);
  printer.write(") {");
  printer.linebreak();
  printer.indent();
  write_node(node->_body, printer);
  printer.linebreak();
  printer.unindent();
  printer.write('}');
}

void Function::write_for(For* node, FormatPrinter& printer) {
  printer.write("for (");
  write_node(node->_init, printer);
  printer.write("; ");
  write_node(node->_cond, printer);
  printer.write("; ");
  write_node(node->_iter, printer);
  printer.write(") {");
  printer.linebreak();
  printer.indent();
  write_node(node->_body, printer);
  printer.linebreak();
  printer.unindent();
  printer.write('}');
}

void Function::write_goto(Goto* node, FormatPrinter& printer) {
  printer.write(fmt::format("goto label_{}", node->_target_id));
}

void Function::write_return(Return* node, FormatPrinter& printer) {
  printer.write("return ");
  write_node(node->_val, printer);
}

void Function::write_routinecall(RoutineCall* node, FormatPrinter& printer) {
  printer.write(fmt::format("sub_{:08x}(", node->_target_va));
  for (size_t i = 0; i < node->_param.size(); i++) {
    if (i > 0) {
      printer.write(", ");
    }
    write_node(node->_param[i], printer);
  }
  printer.write(')');
}

ExprNode* Function::alloc_node_helper(NodeType nt) {
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

void Function::write_pseudocode(std::ostream& sink) {
  FormatPrinter printer(sink, 2);
  printer.write(fmt::format("{} sub_{:08x}(", primitive_type_str(_rtype), _vaddr));
  for (size_t i = 0; i < _params.size(); i++) {
    if (i > 0) {
      printer.write(", ");
    }
    printer.write(fmt::format("{} {}", primitive_type_str(_varmap[_params[i]].first), _varmap[_params[i]].second));
  }
  printer.write(") {");
  printer.linebreak();
  printer.indent();
  write_node(_root, printer);
  printer.linebreak();
  printer.unindent();
  printer.write('}');
}

void Function::translate_ir_routine(ir::IrRoutine const& routine) {
  for (size_t i = 0; i < routine._num_int_param; i++) {
    ir::GPRBindInfo const* gprb = routine._gpr_binds.get_temp(routine._int_param[i]);
    _varmap.emplace_back(ir_type_to_primitive_type(gprb->_type), fmt::format("param_{}", _varmap.size()));
  }
  for (size_t i = 0; i < routine._num_flt_param; i++) {
    ir::FPRBindInfo const* fprb = routine._fpr_binds.get_temp(routine._flt_param[i]);
    _varmap.emplace_back(ir_type_to_primitive_type(fprb->_type), fmt::format("param_{}", _varmap.size()));
  }

  for (uint16_t stkoff : routine._stk_params) {
    // TODO: plumb type info and stack params through
    _varmap.emplace_back(PrimitiveType::kS4, fmt::format("stk_{:x}", stkoff));
  }

  for (size_t i = 0; i < routine._gpr_binds.ntemps(); i++) {
    ir::GPRBindInfo const* gprb = routine._gpr_binds.get_temp(i);
    if (!gprb->_is_param) {
      _varmap.emplace_back(ir_type_to_primitive_type(gprb->_type), fmt::format("i_t{}", _varmap.size()));
    }
  }
  for (size_t i = 0; i < routine._fpr_binds.ntemps(); i++) {
    ir::FPRBindInfo const* fprb = routine._fpr_binds.get_temp(i);
    if (!fprb->_is_param) {
      _varmap.emplace_back(ir_type_to_primitive_type(fprb->_type), fmt::format("f_t{}", _varmap.size()));
    }
  }

  _vaddr = 0;
  _root = alloc_node<Sequence>();

}

Function translate_ir_routine(ir::IrRoutine const& routine) {
  Function ret;
  ret.translate_ir_routine(routine);

  return ret;
}
}  // namespace decomp::hll
