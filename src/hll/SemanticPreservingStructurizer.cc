#include "hll/SemanticPreservingStructurizer.hh"

#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "utl/FlowGraph.hh"
#include "utl/ReservedVector.hh"

namespace decomp::hll {
namespace {
using ACNVertex = FlowVertex<AbstractControlNode*>;
using ACNGraph = FlowGraph<AbstractControlNode*>;

struct SeqSubstitution {
  std::vector<ACNVertex*> _list;
};

struct IfSubstitution {
  ACNVertex* _hdr;
  ACNVertex* _true;
  ACNVertex* _next;
};

struct IfElseSubstitution {
  ACNVertex* _hdr;
  ACNVertex* _true;
  ACNVertex* _false;
  ACNVertex* _next;
};

struct CCondSubstitution {
  std::vector<ACNVertex*> _list;
  ACNVertex* _shortcircuit;
  ACNVertex* _alternative;
  ACNVertex* _next;
};

struct SwitchSubstitution {
  ACNVertex* _hdr;
  std::vector<ACNVertex*> _list;
};

using AcyclicSubstitution =
  std::variant<SeqSubstitution, IfSubstitution, IfElseSubstitution, CCondSubstitution, SwitchSubstitution>;

struct SPSState {
  ACNGraph _gr;
  std::unordered_map<AbstractControlNode*, AbstractControlNode*> _structof;
  std::vector<int> _post;
  std::vector<int> _dom;
  std::vector<int> _pdom;
};

enum class RefinementSuggestion {
  kRefineSwitch,
  kRefineGeneric,
};

using AcyclicResult = Either<RefinementSuggestion, std::pair<AbstractControlNode*, AcyclicSubstitution>>;

// Precondition: vert is a binary split
AcyclicResult compound_boolean(SPSState& state, ACNVertex& vert) {
  CCondSubstitution substitutions;
  ACNGraph& gr = state._gr;
  // Compound conditionals cover a region between a postdominator and its dominator, termed here the "root" and "term"
  // This region's term node should have at least one predecessor which is a single-exit node (implied to be the if/else
  // block) This region's internal nodes should all be ICBS and have the term node be the immediate post-dominator aside
  // from the one or two case nodes

  // First presume that this is a ccond region by finding its bounds
  ACNVertex* term = gr.vertex(state._pdom[vert._idx]);
  ACNVertex* root = gr.vertex(state._dom[term->_idx]);

  // Check that the term node has at least one incoming case node
  bool is_exit_node = false;
  gr.foreach_real_inedge(
    [&is_exit_node](ACNVertex const& v) {
      if (v._out.size() == 1) {
        is_exit_node = true;
      }
    },
    term);
  if (!is_exit_node) {
    return RefinementSuggestion::kRefineGeneric;
  }

  using ReduceGraph = FlowGraph<std::variant<ACNVertex*, int>>;
  using ReduceVertex = FlowVertex<std::variant<ACNVertex*, int>>;
  ReduceGraph ccond_gr;
  ccond_gr.generate_between([&ccond_gr](ACNVertex& vsrc, ReduceVertex& vdst) { vdst._d = &vsrc; }, &gr, root, term);

  int case_count = 0;
  bool is_valid_cc_region = true;
  // Ensure that the subgraph representing this presumed ccond region is well formed
  ccond_gr.foreach_real([&ccond_gr, &state, term, &case_count, &is_valid_cc_region, &substitutions](ReduceVertex& rv) {
    ACNVertex* v = std::get<ACNVertex*>(rv.data());
    // This will pick out internal nodes
    if (v->icbs() && state._pdom[v->_idx] == term->_idx) {
      substitutions._list.push_back(v);
      return;
    }
    // This will pick out the term
    if (v == term) {
      return;
    }
    // This will pick out the 1 or 2 case nodes
    if (v->_out.size() == 1 && v->_out[0]._target == term->_idx) {
      case_count++;
      // Having more than two case nodes would imply this is a proper region
      if (case_count > 2) {
        is_valid_cc_region = false;
      }
    }
    // Having any other node type would imply this is a proper region, or needs further reducing beforehand
    is_valid_cc_region = false;
  });

  if (!is_valid_cc_region) {
    return RefinementSuggestion::kRefineGeneric;
  }

  // Given that this is a CCond, we can begin the reduction process of the internal nodes
  struct ReduceData {
    ReduceVertex* _n0;
    ReduceVertex* _n1;
    EdgeData _n0_n1;
    EdgeData _n0_n2;
    EdgeData _n1_n2;
    EdgeData _n1_n3;
  };
  CCond cond_node;
  bool changed;
  while (true) {
    changed = false;
    std::optional<ReduceData> rdata;
    ccond_gr.foreach_real([&gr, &ccond_gr, &rdata](ReduceVertex& reduce_candidate) {
      auto e0 = reduce_candidate._out[0];
      auto e1 = reduce_candidate._out[1];
      auto v0 = gr.vertex(e0._target);
      auto v1 = gr.vertex(e1._target);
      if (v0->icbs() && v0->_out[0]._target == e1._target) {
        rdata = ReduceData{
          ._n0_n1 = e0,
          ._n0_n2 = e1,
          ._n1_n2 = v0->_out[0],
          ._n1_n3 = v0->_out[1],
        };
      } else if (v0->icbs() && v0->_out[1]._target == e1._target) {
        rdata = ReduceData{
          ._n0_n1 = e0,
          ._n0_n2 = e1,
          ._n1_n2 = v0->_out[1],
          ._n1_n3 = v0->_out[0],
        };
      } else if (v1->icbs() && v1->_out[0]._target == e0._target) {
        rdata = ReduceData{
          ._n0_n1 = e1,
          ._n0_n2 = e0,
          ._n1_n2 = v1->_out[0],
          ._n1_n3 = v1->_out[1],
        };
      } else if (v1->icbs() && v1->_out[1]._target == e0._target) {
        rdata = ReduceData{
          ._n0_n1 = e1,
          ._n0_n2 = e0,
          ._n1_n2 = v1->_out[1],
          ._n1_n3 = v1->_out[0],
        };
      }

      if (rdata) {
        rdata->_n0 = &reduce_candidate;
        rdata->_n1 = ccond_gr.vertex(rdata->_n0_n1._target);
        return false;
      }
      return true;
    });

    if (!rdata) {
      break;
    }

    constexpr auto get_rv_data = [](ReduceVertex* rv) -> std::variant<AbstractControlNode*, int> {
      if (std::holds_alternative<ACNVertex*>(rv->data())) {
        return std::get<ACNVertex*>(rv->data())->data();
      } else {
        return std::get<int>(rv->data());
      }
    };

    int new_cond_tree_vtx = cond_node.put(get_rv_data(rdata->_n0),
      get_rv_data(rdata->_n1),
      // If the short-circuit path condition does not match the fallback condition, then its condition must be inverted
      rdata->_n0_n2._tr != rdata->_n1_n2._tr,
      // If the short-circuit path occurs when true, presume the condition to be an or, otherwise an and
      rdata->_n0_n2._tr == BlockTransfer::kConditionTrue ? CCond::BoolOp::kOr : CCond::BoolOp::kAnd);
    ccond_gr.substitute_pair(std::make_pair(rdata->_n0->_idx, rdata->_n1->_idx), new_cond_tree_vtx);
  }

  return std::make_pair<AbstractControlNode*, AcyclicSubstitution>(new CCond(std::move(cond_node)), substitutions);
}

AcyclicResult acyclic_region_type(SPSState& state, ACNVertex& vert) {
  ACNGraph& gr = state._gr;
  {
    std::vector<AbstractControlNode*> seqlist;
    SeqSubstitution sub;
    if (vert.single_pred()) {
      for (ACNVertex* cur = gr.vertex(vert._in[0]._target); cur->sess(); cur = gr.vertex(cur->_in[0]._target)) {
        sub._list.push_back(cur);
        seqlist.push_back(cur->data());
      }
    }

    std::reverse(sub._list.begin(), sub._list.end());
    std::reverse(seqlist.begin(), seqlist.end());
    sub._list.push_back(&vert);
    seqlist.push_back(vert.data());

    if (vert.single_succ()) {
      for (ACNVertex* cur = gr.vertex(vert._out[0]._target); cur->sess(); cur = gr.vertex(cur->_out[0]._target)) {
        sub._list.push_back(cur);
        seqlist.push_back(cur->data());
      }
    }

    if (seqlist.size() > 1) {
      return std::make_pair<AbstractControlNode*, AcyclicSubstitution>(new Seq(std::move(seqlist)), sub);
    }
  }

  if (vert.icbs()) {
    ACNVertex* m = gr.vertex(vert._out[0]._target);
    ACNVertex* n = gr.vertex(vert._out[1]._target);
    if (m->sess() && n->sess() && gr.vertex(m->_out[0]._target) == gr.vertex(n->_out[0]._target)) {
      if (vert._out[0]._tr == BlockTransfer::kConditionTrue) {
        return std::make_pair<AbstractControlNode*, AcyclicSubstitution>(new IfElse(vert.data(), m->data(), n->data()),
          IfElseSubstitution{
            ._hdr = &vert,
            ._true = m,
            ._false = n,
            ._next = gr.vertex(m->_out[0]._target),
          });
      } else {
        return std::make_pair<AbstractControlNode*, AcyclicSubstitution>(new IfElse(vert.data(), n->data(), m->data()),
          IfElseSubstitution{
            ._hdr = &vert,
            ._true = n,
            ._false = m,
            ._next = gr.vertex(m->_out[0]._target),
          });
      }
    }
    if (m->sess() && gr.vertex(m->_out[0]._target) == n) {
      if (vert._out[0]._tr == BlockTransfer::kConditionTrue) {
        return std::make_pair<AbstractControlNode*, AcyclicSubstitution>(new If(vert.data(), m->data(), false),
          IfSubstitution{
            ._hdr = &vert,
            ._true = m,
            ._next = n,
          });
      } else {
        return std::make_pair<AbstractControlNode*, AcyclicSubstitution>(new If(vert.data(), m->data(), true),
          IfSubstitution{
            ._hdr = &vert,
            ._true = m,
            ._next = n,
          });
      }
    }
    if (n->sess() && gr.vertex(n->_out[0]._target) == m) {
      if (vert._out[1]._tr == BlockTransfer::kConditionTrue) {
        return std::make_pair<AbstractControlNode*, AcyclicSubstitution>(new If(vert.data(), n->data(), false),
          IfSubstitution{
            ._hdr = &vert,
            ._true = n,
            ._next = m,
          });
      } else {
        return std::make_pair<AbstractControlNode*, AcyclicSubstitution>(new If(vert.data(), n->data(), true),
          IfSubstitution{
            ._hdr = &vert,
            ._true = n,
            ._next = m,
          });
      }
    }

    // Either generic refinement or a compound boolean
    return compound_boolean(state, vert);
  }

  // TODO: Check if merging node has multiple inputs, and if so it's probably an if/elseif

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

    // return new Switch(vert.data(), std::move(cases));
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

int replace_in_graph(SPSState& state,
  AcyclicSubstitution const& asub,
  AbstractControlNode* repl,
  AcyclicSubstitution const& sub,
  int post_ctr) {
  const int repl_node_id = state._gr.emplace_vertex(repl);
  state._dom.push_back(-1);
  // Sanity check that everywhere we update the graph, the dom tree is updated to match
  assert(state._dom.size() == state._gr.size());

  switch (repl->_type) {
    case ACNType::Seq: {
      Seq* seq = static_cast<Seq*>(repl);
      SeqSubstitution const& ssub = std::get<SeqSubstitution>(asub);
      ACNVertex* hdr_node = ssub._list.front();
      ACNVertex* tail_node = ssub._list.back();

      // Link nodes entering this structure to the new node
      for (auto inedge : hdr_node->_in) {
        state._gr.emplace_link(inedge._target, repl_node_id, inedge._tr);
      }
      for (auto outedge : tail_node->_out) {
        state._gr.emplace_link(repl_node_id, outedge._target, outedge._tr);
      }

      // Detach and mark parent structure
      for (auto node : ssub._list) {
        state._gr.detach(node);
        state._structof[node->data()] = repl;
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
      // TODO: finish the fixup and test

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

    case ACNType::CCond: {
      CCond* ccond = static_cast<CCond*>(repl);
      std::vector<int> repl_nodes;
      std::transform(
        ccond->_leaves.begin(), ccond->_leaves.end(), std::back_inserter(repl_nodes), [&state](AbstractControlNode* n) {
          return state._data2vert[n]->_idx;
        });
      // TODO: replace every single thing in this function with a call to "substitute in graph"
      // TODO: flesh out graph substitution algorithm a bit more?
      return 0;
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
  state._gr.copy_shape_generator(
    &_routine->_graph, [this, &state](ir::IrBlockVertex const& ibv, ACNVertex& acnv) { acnv._d = _leaves[ibv._idx]; });
  state._dom = state._gr.compute_dom_tree();

  do {
    state._post.clear();
    state._gr.postorder_fwd([&state](ACNVertex const& acnv) { state._post.push_back(acnv._idx); }, state._gr.root());

    for (int post_ctr = 0; post_ctr < static_cast<int>(state._post.size()) && state._gr.size() > 1;) {
      ACNVertex* vert = state._gr.vertex(state._post[post_ctr]);
      AcyclicResult acyclic_result = acyclic_region_type(state, *vert);

      if (acyclic_result) {
        post_ctr = replace_in_graph(state, acyclic_result.right().first, acyclic_result.right().second, post_ctr);
        continue;
      }

      // std::vector<int> loop_memb = loop_membership(state, vert);
      // if (!loop_memb.empty()) {
      //   Either<RefinementSuggestion, AbstractControlNode*> cyclic_result = cyclic_region_type(state, *vert,
      //   loop_memb);
      // }
    }
  } while (state._gr.size() > 1);

  return HLLControlTree{};
}
}  // namespace decomp::hll
