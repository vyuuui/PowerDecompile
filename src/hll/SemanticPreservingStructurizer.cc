#include "hll/SemanticPreservingStructurizer.hh"

#include <vector>

#include "utl/FlowGraph.hh"

namespace decomp::hll {
namespace {
using ACNVertex = FlowVertex<AbstractControlNode*>;
using ACNGraph = FlowGraph<AbstractControlNode*>;

enum class RefinementSuggestion {
  kRefineSwitch,
  kRefineGeneric,
};

Either<RefinementSuggestion, AbstractControlNode*> acyclic_region_type(ACNGraph& gr, ACNVertex& vert) {
  std::vector<AbstractControlNode*> seqlist;
  if (vert.single_succ()) {
    for (ACNVertex* cur = gr.vertex(vert._out[0]._target); cur->sess(); cur = gr.vertex(cur->_out[0]._target)) {
      seqlist.push_back(cur->data());
    }
  }

  if (vert.single_pred()) {
    for (ACNVertex* cur = gr.vertex(vert._in[0]._target); cur->sess(); cur = gr.vertex(cur->_in[0]._target)) {
      seqlist.push_back(cur->data());
    }
  }

  if (!seqlist.empty()) {
    return new Seq(std::move(seqlist));
  }

  if (vert._out.size() == 2 && inverse_condition(vert._out[0]._tr, vert._out[1]._tr)) {
    ACNVertex const* m = gr.vertex(vert._out[0]._target);
    ACNVertex const* n = gr.vertex(vert._out[1]._target);
    if (m->sess() && n->sess() && gr.vertex(m->_out[0]._target) == gr.vertex(n->_out[0]._target)) {
      if (vert._out[0]._tr == BlockTransfer::kConditionTrue) {
        return new IfElse(vert.data(), m->data(), n->data());
      } else {
        return new IfElse(vert.data(), n->data(), m->data());
      }
    }
    if (m->sess() && gr.vertex(m->_out[0]._target) == n) {
      if (vert._out[0]._tr == BlockTransfer::kConditionTrue) {
        return new If(vert.data(), m->data(), false);
      } else {
        return new If(vert.data(), m->data(), true);
      }
    }
    if (n->sess() && gr.vertex(n->_out[0]._target) == m) {
      if (vert._out[1]._tr == BlockTransfer::kConditionTrue) {
        return new If(vert.data(), n->data(), false);
      } else {
        return new If(vert.data(), n->data(), true);
      }
    }

    // General refinement
    return RefinementSuggestion::kRefineGeneric;
  }

  // >= 2 outedges, non-inverse conditions
  if (vert._out.size() >= 2) {
    ACNVertex* s0 = gr.vertex(vert._out[0]._target);
    if (!s0->sess()) {
      // Switch candidate
      return RefinementSuggestion::kRefineSwitch;
    }
    ACNVertex* follow_node = gr.vertex(s0->_out[0]._target);

    std::vector<std::pair<BlockTransfer, AbstractControlNode*>> cases;
    cases.emplace_back(vert._out[0]._tr, s0->data());
    for (size_t i = 1; i < vert._out.size(); i++) {
      ACNVertex* sn = gr.vertex(vert._out[i]._target);
      if (!sn->single_succ() || !sn->single_pred()) {
        // Switch candidate
        return RefinementSuggestion::kRefineSwitch;
      }
      ACNVertex* sn_succ = gr.vertex(sn->_out[0]._target);
      if (sn_succ != follow_node) {
        // Switch candidate
        return RefinementSuggestion::kRefineSwitch;
      }
      cases.emplace_back(vert._out[i]._tr, sn->data());
    }

    return new Switch(vert.data(), std::move(cases));
  }

  // General refinement
  return RefinementSuggestion::kRefineGeneric;
}
}  // namespace

HLLControlTree SemanticPreservingStructurizer::structurize() {
  ACNGraph gr;
  gr.copy_shape_generator(
    &_routine->_graph, [this](ir::IrBlockVertex const& ibv, ACNVertex& acnv) { acnv._d = _leaves[ibv._idx]; });
  do {
    std::vector<int> post;
    gr.postorder_fwd([&post](ACNVertex const& acnv) { post.push_back(acnv._idx); }, gr.root());

    for (int post_ctr = 0; post_ctr < static_cast<int>(post.size()) && gr.size() > 1;) {
      ACNVertex* vert = gr.vertex(post[post_ctr]);
      Either<RefinementSuggestion, AbstractControlNode*> node = acyclic_region_type(gr, *vert);

      if (node) {
        // add node to graph, remove things added
      } else {
        // continue to switch refinement and loop detection
      }
    }
  } while (gr.size() > 1);

  return HLLControlTree{};
}
}  // namespace decomp::hll
