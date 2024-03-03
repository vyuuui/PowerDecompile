#include "hll/SemanticPreservingStructurizer.hh"

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "utl/FlowGraph.hh"

namespace decomp::hll {
namespace {
using ACNVertex = FlowVertex<AbstractControlNode*>;
using ACNGraph = FlowGraph<AbstractControlNode*>;

struct SPSState {
  ACNGraph _gr;
  std::unordered_map<AbstractControlNode*, ACNVertex*> _data2vert;
  std::unordered_map<AbstractControlNode*, AbstractControlNode*> _structof;
  std::vector<int> _post;
  std::vector<int> _dom;
  std::vector<int> _pdom;
};

enum class RefinementSuggestion {
  kRefineSwitch,
  kRefineGeneric,
};

Either<RefinementSuggestion, AbstractControlNode*> acyclic_region_type(SPSState& state, ACNVertex& vert) {
  ACNGraph& gr = state._gr;
  std::vector<AbstractControlNode*> seqlist;
  if (vert.single_pred()) {
    for (ACNVertex* cur = gr.vertex(vert._in[0]._target); cur->sess(); cur = gr.vertex(cur->_in[0]._target)) {
      seqlist.push_back(cur->data());
    }
  }

  std::reverse(seqlist.begin(), seqlist.end());
  seqlist.push_back(vert.data());

  if (vert.single_succ()) {
    for (ACNVertex* cur = gr.vertex(vert._out[0]._target); cur->sess(); cur = gr.vertex(cur->_out[0]._target)) {
      seqlist.push_back(cur->data());
    }
  }

  if (seqlist.size() > 1) {
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

Either<RefinementSuggestion, AbstractControlNode*> cyclic_region_type(
  SPSState& state, ACNVertex& vert, std::vector<int> const& loop_members) {
  for (auto [in_idx, _] : vert._in) {
    if (in_idx == vert._idx) {
      return new SelfLoop(vert.data());
    }
  }

  // Do-while = self loop!
  if (vert._out.size() == 1) {
    // Hypothesis: do-while
    
  } else if (vert._out.size() == 2) {
    // Hypothesis: while
    ACNVertex* follow;
    if (std::find(loop_members.begin(), loop_members.end(), vert._out[0]._target) != loop_members.end()) {
      follow = state._gr.vertex(vert._out[0]._target);
    } else {
      assert(std::find(loop_members.begin(), loop_members.end(), vert._out[1]._target) != loop_members.end());
      follow = state._gr.vertex(vert._out[1]._target);
    }

    if (follow->_out.size()) {
    }
  }
}

int replace_in_graph(SPSState& state, AbstractControlNode* repl, int post_ctr) {
  const int repl_node_id = state._gr.emplace_vertex(repl);
  state._data2vert[repl] = state._gr.vertex(repl_node_id);
  state._dom.push_back(-1);
  // Sanity check that everywhere we update the graph, the dom tree is updated to match
  assert(state._dom.size() == state._gr.size());

  switch (repl->_type) {
    case ACNType::Seq: {
      Seq* seq = static_cast<Seq*>(repl);
      ACNVertex* hdr_node = state._data2vert[seq->_seq.front()];
      ACNVertex* tail_node = state._data2vert[seq->_seq.back()];

      // Link nodes entering this structure to the new node
      for (auto inedge : hdr_node->_in) {
        state._gr.emplace_link(inedge._target, repl_node_id, inedge._tr);
      }
      for (auto outedge : tail_node->_out) {
        state._gr.emplace_link(repl_node_id, outedge._target, outedge._tr);
      }

      // Detach and mark parent structure
      for (auto node : seq->_seq) {
        state._gr.detach(state._data2vert[node]);
        state._structof[node] = repl;
      }

      // Update the dominator tree
      // Since this is a sequential list of nodes, the only dominators needing updating are nodes dominated by the tail
      std::replace(state._dom.begin(), state._dom.end(), tail_node->_idx, repl_node_id);
      // And the dominator of this structure is the dominator of the header node
      state._dom[repl_node_id] = state._dom[hdr_node->_idx];
      // Clear out old dominator info
      for (auto seqnode : seq->_seq) {
        state._dom[state._data2vert[seqnode]->_idx] = -1;
      }

      // Update the postorder index to point to this new node
      int seq_end = post_ctr;
      if (state._post[post_ctr] != hdr_node->_idx) {
        // Header node can't possibly appear before the current postorder counter
        seq_end = *std::find(state._post.begin() + post_ctr, state._post.end(), hdr_node->_idx);
      }
      state._post[seq_end] = repl_node_id;
      // Since this is a sequential list of nodes, they must all appear sequentially in the postorder iteration
      const int seq_begin = seq_end - seq->_seq.size() + 1;
      state._post.erase(state._post.begin() + seq_begin, state._post.begin() + seq_end);

      return seq_begin;
    }

    case ACNType::If: {
      If* ifn = static_cast<If*>(repl);
      ACNVertex* hdr_node = state._data2vert[ifn->_head];
      ACNVertex* tail_node = state._data2vert[ifn->_true];

      // Sanity check
      assert(hdr_node->_out.size() == 2);
      assert(tail_node->_out.size() == 1);

      // Link nodes entering this structure to the new node
      for (auto inedge : hdr_node->_in) {
        state._gr.emplace_link(inedge._target, repl_node_id, inedge._tr);
      }
      state._gr.emplace_link(repl_node_id, tail_node->_out[0]._target, tail_node->_out[0]._tr);

      // Detach and mark parent structure
      state._gr.detach(hdr_node);
      state._gr.detach(tail_node);
      state._structof[ifn->_head] = repl;
      state._structof[ifn->_true] = repl;

      // Update the dominator tree
      // Since this is an If node, the 'true' block should be a leaf in the dominator tree, thus anything dominated by
      // this structure is dominated only by the header
      std::replace(state._dom.begin(), state._dom.end(), hdr_node->_idx, repl_node_id);
      // And the dominator of this structure is the dominator of the header node (again)
      state._dom[repl_node_id] = state._dom[hdr_node->_idx];
      // Clear out old dominator info
      state._dom[hdr_node->_idx] = -1;
      state._dom[tail_node->_idx] = -1;

      int hdr_post_idx = post_ctr;
      if (state._post[post_ctr] != hdr_node->_idx) {
        hdr_post_idx = *std::find(state._post.begin() + post_ctr, state._post.end(), hdr_node->_idx);
      }
      state._post[hdr_post_idx] = repl_node_id;
      // Finding the true block of an if is a bit harder, as it could be the immediate previous postorder node, or it
      // could be much further away
      if (state._post[hdr_post_idx - 1] == tail_node->_idx) {
        state._post.erase(state._post.begin() + hdr_post_idx - 1);
      } else {
        state._post.erase(std::find(state._post.begin(), state._post.begin() + post_ctr, tail_node->_idx));
      }
      return hdr_post_idx - 1;
    }

    case ACNType::IfElse: {
      IfElse* ifelse = static_cast<IfElse*>(repl);
      ACNVertex* hdr_node = state._data2vert[ifelse->_head];
      ACNVertex* tail1 = state._data2vert[ifelse->_true];
      ACNVertex* tail2 = state._data2vert[ifelse->_false];

      // Sanity check
      assert(hdr_node->_out.size() == 2);
      assert(tail1->_out.size() == 1);
      assert(tail2->_out.size() == 1);

      // Link nodes entering this structure to the new node
      for (auto inedge : hdr_node->_in) {
        state._gr.emplace_link(inedge._target, repl_node_id, inedge._tr);
      }
      // Since tail1.out == tail2.out only need one link emplaced
      state._gr.emplace_link(repl_node_id, tail1->_out[0]._target, tail1->_out[0]._tr);

      // Detach and mark parent structure
      state._gr.detach(hdr_node);
      state._gr.detach(tail1);
      state._gr.detach(tail2);
      state._structof[ifelse->_head] = repl;
      state._structof[ifelse->_true] = repl;
      state._structof[ifelse->_false] = repl;

      // Update the dominator tree
      // Same rule in the If case applies for IfElse
      std::replace(state._dom.begin(), state._dom.end(), hdr_node->_idx, repl_node_id);
      state._dom[repl_node_id] = state._dom[hdr_node->_idx];
      // Clear out old dominator info
      state._dom[hdr_node->_idx] = -1;
      state._dom[tail1->_idx] = -1;
      state._dom[tail2->_idx] = -1;

      int hdr_post_idx = post_ctr;
      if (state._post[post_ctr] != hdr_node->_idx) {
        hdr_post_idx = *std::find(state._post.begin() + post_ctr, state._post.end(), hdr_node->_idx);
      }
      state._post[hdr_post_idx] = repl_node_id;

      // One of the case nodes must be the header's postorder predecessor, the other one must be searched for
      if (state._post[hdr_post_idx - 1] == tail1->_idx) {
        state._post.erase(std::find(state._post.begin(), state._post.begin() + post_ctr, tail2->_idx));
      } else {  // == tail2->_idx
        state._post.erase(std::find(state._post.begin(), state._post.begin() + post_ctr, tail1->_idx));
      }
      state._post.erase(state._post.begin() + hdr_post_idx - 1);
      return hdr_post_idx - 2;
    }

    case ACNType::Switch: {
      Switch* switchn = static_cast<Switch*>(repl);
      ACNVertex* hdr_node = state._data2vert[switchn->_head];
      ACNVertex* first_case_node = state._data2vert[switchn->_cases[0].second];

      // Link nodes entering this structure to the new node
      for (auto inedge : hdr_node->_in) {
        state._gr.emplace_link(inedge._target, repl_node_id, inedge._tr);
      }
      // Switch schema case nodes must lead to the same merge point, therefore
      // _cases[0].out == _cases[1].out == ... == cases[n].out
      state._gr.emplace_link(repl_node_id, first_case_node->_out[0]._target, first_case_node->_out[0]._tr);

      // Detach and mark parent structure
      state._gr.detach(hdr_node);
      state._structof[switchn->_head] = repl;
      for (auto [_, casen] : switchn->_cases) {
        state._gr.detach(state._data2vert[casen]);
        state._structof[casen] = repl;
      }

      // Update the dominator tree
      // Same rule in the If and IfElse case applies to Switches too
      std::replace(state._dom.begin(), state._dom.end(), hdr_node->_idx, repl_node_id);
      state._dom[repl_node_id] = state._dom[hdr_node->_idx];
      // Clear out old dominator info
      state._dom[hdr_node->_idx] = -1;
      for (auto [_, casen] : switchn->_cases) {
        state._dom[state._data2vert[casen]->_idx] = -1;
      }

      int hdr_post_idx = post_ctr;
      if (state._post[post_ctr] != hdr_node->_idx) {
        hdr_post_idx = *std::find(state._post.begin() + post_ctr, state._post.end(), hdr_node->_idx);
      }
      state._post[hdr_post_idx] = repl_node_id;

      // Instead of thinking hard about this, just bruteforce search for all case nodes and remove them
      for (auto [_, c] : switchn->_cases) {
        ACNVertex* casen = state._data2vert[c];
        state._post.erase(std::find(state._post.begin(), state._post.begin() + hdr_post_idx, casen->_idx));
        hdr_post_idx--;
      }
      return hdr_post_idx;
    }

    default:
      return -1;
  }
}

enum PathCacheState {
  kUndecided,
  kPathBack,
  kNoPathBack,
};

std::vector<int> loop_membership(SPSState& state, ACNVertex* head) {
  enum class Reachability : uint8_t {
    kUnprocessed,
    kPathBack,
    kNoPathBack,
  };
  std::vector<Reachability> reachability(state._gr.size(), Reachability::kUnprocessed);
  std::vector<int> reach_under;

  bool is_header = false;
  // Determine backedges of header
  for (auto [in_idx, _] : head->_in) {
    if (dominates(state._dom, head->_idx, in_idx)) {
      reachability[in_idx] = Reachability::kPathBack;
      is_header = true;
    }
  }
  if (!is_header) {
    return reach_under;
  }
  reachability[head->_idx] = Reachability::kNoPathBack;

  state._gr.postorder_fwd(
    [&reach_under, &reachability](ACNVertex& v) {
      Reachability r = Reachability::kNoPathBack;
      for (auto [out_idx, _] : v._out) {
        if (reachability[out_idx] == Reachability::kPathBack) {
          r = Reachability::kPathBack;
          reach_under.push_back(v._idx);
        }
      }
      reachability[v._idx] = r;
    },
    head);

  reach_under.push_back(head->_idx);
  return reach_under;
}
}  // namespace

HLLControlTree SemanticPreservingStructurizer::structurize() {
  SPSState state;
  // Copy IR graph shape over the ACN graph, assigning the set of basic blocks as leaves
  state._gr.copy_shape_generator(&_routine->_graph, [this, &state](ir::IrBlockVertex const& ibv, ACNVertex& acnv) {
    acnv._d = _leaves[ibv._idx];
    if (acnv.is_real()) {
      state._data2vert[acnv.data()] = &acnv;
    }
  });
  state._dom = state._gr.compute_dom_tree();

  do {
    state._post.clear();
    state._gr.postorder_fwd([&state](ACNVertex const& acnv) { state._post.push_back(acnv._idx); }, state._gr.root());

    for (int post_ctr = 0; post_ctr < static_cast<int>(state._post.size()) && state._gr.size() > 1;) {
      ACNVertex* vert = state._gr.vertex(state._post[post_ctr]);
      Either<RefinementSuggestion, AbstractControlNode*> acyclic_result = acyclic_region_type(state, *vert);

      if (acyclic_result) {
        post_ctr = replace_in_graph(state, acyclic_result.right(), post_ctr);
        continue;
      }

      std::vector<int> loop_memb = loop_membership(state, vert);
      if (!loop_memb.empty()) {
        Either<RefinementSuggestion, AbstractControlNode*> cyclic_result = cyclic_region_type(state, *vert, loop_memb);
      }
    }
  } while (state._gr.size() > 1);

  return HLLControlTree{};
}
}  // namespace decomp::hll
