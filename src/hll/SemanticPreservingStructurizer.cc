#include "hll/SemanticPreservingStructurizer.hh"

#include <vector>

#include "utl/FlowGraph.hh"

namespace decomp::hll {
namespace {
using ACNVertex = FlowVertex<AbstractControlNode*>;
using ACNGraph = FlowGraph<AbstractControlNode*>;

enum class AcyclicStructure {
  kSeq,
  kIf,
  kIfElse,
  kIncSwitch,
  kSwitch,
  kSwitchCandidate,
  kUnstructured,
};

AcyclicStructure acyclic_region_type(ACNGraph const& gr, ACNVertex const& vert, std::vector<ACNVertex const*>& vset) {
  ACNVertex const* cur = &vert;
  bool single_pred = true, single_succ = cur->_out.size() == 1;
  while (single_pred && single_succ) {
    vset.push_back(cur);
    cur = gr.vertex(cur->_out[0]._target);
    single_pred = cur->single_pred();
    single_succ = cur->single_succ();
  }
  if (single_pred) {
    vset.push_back(cur);
  }

  cur = &vert;
  single_pred = cur->_in.size() == 1;
  single_succ = true;
  while (single_pred && single_succ) {
    vset.push_back(cur);
    cur = gr.vertex(cur->_in[0]._target);
    single_pred = cur->single_pred();
    single_succ = cur->single_succ();
  }
  if (single_succ) {
    vset.push_back(cur);
  }

  vset.erase(std::unique(vset.begin(), vset.end()), vset.end());

  if (vset.size() > 1) {
    return AcyclicStructure::kSeq;
  }

  vset.clear();
  ACNVertex const* feature_head = cur;
  if (feature_head->_out.size() == 2 && inverse_condition(feature_head->_out[0]._tr, feature_head->_out[1]._tr)) {
    ACNVertex const* m = gr.vertex(cur->_out[0]._target);
    ACNVertex const* n = gr.vertex(cur->_out[1]._target);
    if (m->single_succ() && n->single_succ() && gr.vertex(m->_out[0]._target) == gr.vertex(n->_out[0]._target)) {
      vset.push_back(feature_head);
      vset.push_back(m);
      vset.push_back(n);
      return AcyclicStructure::kIfElse;
    }
    if (m->single_succ() && gr.vertex(m->_out[0]._target) == n) {
      vset.push_back(feature_head);
      vset.push_back(m);
      return AcyclicStructure::kIf;
    }
    if (n->single_succ() && gr.vertex(n->_out[0]._target) == m) {
      vset.push_back(feature_head);
      vset.push_back(n);
      return AcyclicStructure::kIf;
    }
    // This node is a refinement candidate
    return AcyclicStructure::kUnstructured;
  }

  // >= 2 outedges, non-inverse conditions
  if (feature_head->_out.size() >= 2) {
    ACNVertex const* s0 = gr.vertex(feature_head->_out[0]._target);
    if (!s0->single_succ() || !s0->single_pred()) {
      return AcyclicStructure::kSwitchCandidate;
    }
    ACNVertex const* follow_node = gr.vertex(s0->_out[0]._target);
    for (size_t i = 1; i < feature_head->_out.size(); i++) {
      ACNVertex const* sn = gr.vertex(feature_head->_out[i]._target);
      if (!sn->single_succ() || !sn->single_pred()) {
        return AcyclicStructure::kSwitchCandidate;
      }
      ACNVertex const* sn_succ = gr.vertex(sn->_out[0]._target);
      if (sn_succ != follow_node) {
        return AcyclicStructure::kSwitchCandidate;
      }
    }
  }
}
}  // namespace

HLLControlTree SemanticPreservingStructurizer::structurize() {
  FlowGraph<AbstractControlNode*> gr;
  gr.copy_shape_generator(
    &_routine->_graph, [this](ir::IrBlockVertex const& ibv, ACNVertex& acnv) { acnv._d = _leaves[ibv._idx]; });
  do {
    std::vector<int> post;
    gr.postorder_fwd([&post](ACNVertex const& acnv) { post.push_back(acnv._idx); }, gr.root());

    for (int post_ctr = 0; post_ctr < post.size() && gr.size() > 1;) {
      ACNVertex* vert = gr.vertex(post[post_ctr]);
    }
  } while (gr.size() > 1);
}
}  // namespace decomp::hll
