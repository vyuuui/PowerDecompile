#pragma once

#include <cassert>
#include <ostream>
#include <string>
#include <vector>

#include "ir/GekkoTranslator.hh"
#include "hll/ExprNode.hh"

namespace decomp::hll {
class FormatPrinter;

class Function {
private:
  uint32_t _vaddr;
  Sequence* _root;
  std::vector<ExprNode*> _node_list;
  std::vector<bool> _node_has_lbl;
  std::vector<std::pair<PrimitiveType, std::string>> _varmap;
  std::vector<uint32_t> _params;
  PrimitiveType _rtype;

private:
  ExprNode* lookup(NodeRef node) { return _node_list[node]; }

  void write_node(NodeRef node, FormatPrinter& printer);
  void write_node(ExprNode* node, FormatPrinter& printer);

  void write_sequence(Sequence* node, FormatPrinter& printer);
  void write_declaration(Declaration* node, FormatPrinter& printer);
  void write_typeconvert(TypeConvert* node, FormatPrinter& printer);
  void write_primitiveimm(PrimitiveImm* node, FormatPrinter& printer);
  void write_variable(Variable* node, FormatPrinter& printer);
  void write_assignment(Assignment* node, FormatPrinter& printer);
  void write_binaryop(BinaryOp* node, FormatPrinter& printer);
  void write_unaryop(UnaryOp* node, FormatPrinter& printer);
  void write_if(If* node, FormatPrinter& printer);
  void write_while(While* node, FormatPrinter& printer);
  void write_for(For* node, FormatPrinter& printer);
  void write_goto(Goto* node, FormatPrinter& printer);
  void write_return(Return* node, FormatPrinter& printer);
  void write_routinecall(RoutineCall* node, FormatPrinter& printer);

  template <typename T>
  T* alloc_node() {
    return reinterpret_cast<T*>(alloc_node_helper(T::kStaticType));
  }
  ExprNode* alloc_node_helper(NodeType nt);

  friend Function translate_ir_routine(ir::IrRoutine const& routine);

  void translate_ir_block(ir::IrBlock const& irb, Sequence* scope);
  void translate_ir_routine(ir::IrRoutine const& routine);

public:
  void write_pseudocode(std::ostream& sink);
};

Function translate_ir_routine(ir::IrRoutine const& routine);
}  // namespace decomp::hll
