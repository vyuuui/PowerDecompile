#pragma once

#include "ir/GekkoTranslator.hh"

namespace decomp::hll {
struct AbstractControlNode;

struct HLLControlTree {
  AbstractControlNode* _root;
};

// Generic control flow structurizer, transforms an IrGraph into a structured AST
class ControlFlowStructurizer {
public:
  virtual HLLControlTree structurize() = 0;

  // This is called by run_control_flow_analysis
  void prepare(ir::IrRoutine const& routine);

protected:
  // IR Graph to structurize
  ir::IrRoutine const* _routine;
  // Initial node set
  std::vector<AbstractControlNode*> _leaves;
};

enum class ACNType {
  Basic,
  Seq,
  If,
  IfElse,
  IfElseIf,
  Switch,
  DoWhile,
  While,
  For,
  SelfLoop,
  Goto,
  Tail,
};

struct AbstractControlNode {
  AbstractControlNode(ACNType type) : _type(type) {}
  ACNType _type;
};

template <ACNType NT>
struct StaticTypedACN : AbstractControlNode {
  StaticTypedACN() : AbstractControlNode(NT) {}
  inline static constexpr ACNType kStaticType = NT;
};

// Wrapper for IR blocks, leaves of AST
struct BasicBlock : StaticTypedACN<ACNType::Basic> {
  BasicBlock(ir::IrBlockVertex const& irb) : _irb(irb) {}
  ir::IrBlockVertex const& _irb;

  constexpr ir::IrBlockVertex const& operator*() const { return _irb; }

  constexpr ir::IrBlockVertex const* operator->() const { return &_irb; }
};

// Sequence of abstract nodes
struct Seq : StaticTypedACN<ACNType::Seq> {
  Seq() {}
  Seq(std::vector<AbstractControlNode*>&& seq) : _seq(std::move(seq)) {}
  std::vector<AbstractControlNode*> _seq;
};

// If-sans-else schema
struct If : StaticTypedACN<ACNType::If> {
  If() : _head(nullptr), _true(nullptr), _invert(false) {}
  If(AbstractControlNode* head, AbstractControlNode* t, bool invert) : _head(head), _true(t), _invert(invert) {}
  AbstractControlNode* _head;
  AbstractControlNode* _true;
  bool _invert;
};

// If-else schema
struct IfElse : StaticTypedACN<ACNType::IfElse> {
  IfElse() : _head(nullptr), _true(nullptr), _false(nullptr) {}
  IfElse(AbstractControlNode* head, AbstractControlNode* t, AbstractControlNode* f)
      : _head(head), _true(t), _false(f) {}
  AbstractControlNode* _head;
  AbstractControlNode* _true;
  AbstractControlNode* _false;
};

// If-elseif-else schema
struct IfElseIf : StaticTypedACN<ACNType::IfElseIf> {
  std::vector<std::pair<AbstractControlNode*, AbstractControlNode*>> _conds;
  std::optional<AbstractControlNode*> _fallthrough;
};

// Switch schema
struct Switch : StaticTypedACN<ACNType::Switch> {
  Switch() : _head(nullptr) {}
  Switch(AbstractControlNode* head, std::vector<std::pair<BlockTransfer, AbstractControlNode*>>&& cases)
      : _head(nullptr), _cases(std::move(cases)) {}
  AbstractControlNode* _head;
  std::vector<std::pair<BlockTransfer, AbstractControlNode*>> _cases;
};

// Do-while schema
struct DoWhile : StaticTypedACN<ACNType::DoWhile> {
  AbstractControlNode* _body;
  AbstractControlNode* _cond;
};

// While schema
struct While : StaticTypedACN<ACNType::While> {
  AbstractControlNode* _cond;
  AbstractControlNode* _body;
};

// For schema
struct For : StaticTypedACN<ACNType::For> {
  AbstractControlNode* _init;
  AbstractControlNode* _cond;
  AbstractControlNode* _body;
  AbstractControlNode* _it;
};

struct SelfLoop : StaticTypedACN<ACNType::SelfLoop> {
  SelfLoop() : _n(nullptr) {}
  SelfLoop(AbstractControlNode* n) : _n(n) {}
  AbstractControlNode* _n;
};

// TODO: remove structurizer parameter, refer to it from global options perhaps?
HLLControlTree run_control_flow_analysis(ControlFlowStructurizer* structurizer, ir::IrRoutine const& routine);
}  // namespace decomp::hll
