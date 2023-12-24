#include "hll/CFAnalyzer.hh"

#include <limits>
#include <numeric>
#include <set>
#include <vector>

namespace decomp::hll {
namespace {
struct NaturalLoop {
  // loop head
  ir::IrBlock const* _header;
  // Backedges of this loop for this loop
  std::vector<ir::BlockTransition const*> _backedge;
  // Edges which exit the loop
  std::vector<ir::BlockTransition const*> _exits;
  // LCA of all exit points, to determine the post-loop section
  // This can be derived from the earliest common post-dominator of K nodes
  ir::IrBlock const* _exit_lca;
};

struct CFAnalysisData {
  // Immediate Dominator, _idom[i] = j => j idom i
  std::vector<int> _idom;
  // Immediate Post-Dominator, _ipdom[i] = j => j pdom i
  std::vector<int> _ipdom;

  // Natural loops
  std::vector<NaturalLoop> _loops;
  // Loop membership
  std::vector<std::vector<int>> _lmemb;
};

int lca_node(std::vector<int> const& dom_tree, std::vector<ir::BlockTransition const*> const& exits) {
  std::vector<int> visits(dom_tree.size());
  std::vector<int> trace_locations;
  // Init all traces to exit points
  for (auto tr : exits) {
    trace_locations.push_back(tr->_src_idx);
  }

  std::optional<int> lca;
  // Lockstep iterate all traces outward until a node is hit which has a visited count == number of exit nodes
  // This should be the least common ancestor
  while (!lca) {
    for (int& node : trace_locations) {
      if (++visits[node] == exits.size()) {
        lca = node;
        break;
      }

      node = dom_tree[node];
    }
  }

  return *lca;
}

class DisjointSet {
public:
  DisjointSet(size_t len) : _ds(len) { std::iota(_ds.begin(), _ds.end(), 0); }

  bool isroot(int v) const { return _ds[v] == v; }

  void link(int from, int to) {
    int fr = root(from);
    int tr = root(to);
    _ds[fr] = _ds[tr];
  }

  int root(int node) {
    compress(node);
    // After path compression, _ds[i] = i or root(i)
    return _ds[node];
  }

private:
  void compress(int from) {
    int r = root_nocompress(from);
    while (_ds[from] != r) {
      int nxt = _ds[from];
      _ds[from] = r;
      from = nxt;
    }
  }

  int root_nocompress(int node) {
    while (_ds[node] != node) {
      node = _ds[node];
    }
    return node;
  }

  std::vector<int> _ds;
};

void lengauer_tarjan(ir::IrRoutine const& routine, CFAnalysisData& data) {
  const size_t nblks = routine._blocks.size();
  std::vector<int> idom(nblks, 0);

  // Notation and symbols:
  //   G = Control flow graph
  //   T = DFS tree
  //   *_gr -> Graph node number
  //   *_dfs -> DFS tree number

  // Step 1: compute dfs number and tree for all v ∈ G
  // stores DFS->vert in sdom, vert->DFS in dfs2gr
  std::vector<int> sdom(nblks, -1);
  std::vector<int> dfs2vert(nblks);
  std::vector<int> parent(nblks);
  std::vector<std::pair<ir::IrBlock const*, int>> proc;
  proc.emplace_back(&routine._blocks[routine._root_id], 0);

  int dfs_num = 0;
  while (!proc.empty()) {
    auto [block, parent_gr] = proc.back();
    proc.pop_back();
    sdom[block->_id] = dfs_num;
    dfs2vert[dfs_num] = block->_id;
    parent[block->_id] = parent_gr;

    for (ir::BlockTransition const& succ : block->_tr_out) {
      if (sdom[succ._target_idx] < 0) {
        proc.emplace_back(&routine._blocks[succ._target_idx], block->_id);
      }
    }
    dfs_num++;
  }

  // Step 2: compute semidominator into sdom(v) for v ∈ T
  // Step 3: find immediate dominators
  std::vector<std::set<int>> bucket(nblks);
  DisjointSet forest(nblks);
  const auto min_vert = [&parent, &forest, &sdom](int v) {
    int r = forest.root(v);
    if (r == v) {
      return v;
    }

    // Find the vertex "u" with minimal semidominator along r->v
    int min_u = v;
    for (int u = parent[v]; u != r; u = parent[u]) {
      if (sdom[u] < sdom[min_u]) {
        min_u = u;
      }
    }
    return min_u;
  };

  for (int w_dfs = nblks - 1, w_gr = dfs2vert[nblks - 1]; w_dfs > 0; w_gr = dfs2vert[--w_dfs]) {
    ir::IrBlock const& w_blk = routine._blocks[w_gr];

    for (int v_gr : w_blk._tr_in) {
      const int u_gr = min_vert(v_gr);
      sdom[w_gr] = std::min(sdom[w_gr], sdom[u_gr]);
    }

    forest.link(w_gr, parent[w_gr]);
    bucket[dfs2vert[sdom[w_gr]]].insert(w_gr);

    for (int v_gr : bucket[parent[w_dfs]]) {
      const int u_gr = min_vert(v_gr);
      // idom is either known now (parent[w]), or must be deferred (defer to idom[u])
      idom[v_gr] = sdom[u_gr] < sdom[v_gr] ? u_gr : parent[w_gr];
    }
    bucket[parent[w_dfs]].clear();
  }

  // Step 4: forward iterate to complete idom for missed entries
  for (int w_dfs = 1, w_gr = dfs2vert[1]; w_dfs < nblks; w_gr = dfs2vert[++w_dfs]) {
    if (idom[w_gr] != dfs2vert[sdom[w_gr]]) {
      // feedforward immediate dominator deferred earlier
      idom[w_gr] = idom[idom[w_gr]];
    }
  }

  data._idom = std::move(idom);
}

// Preprocess an IR routine for all loops
CFAnalysisData preprocess_routine(ir::IrRoutine const& routine) {
  constexpr int kUnvisited = -1;
  constexpr int kNotLoopHeader = -2;

  CFAnalysisData ret;
  lengauer_tarjan(routine, ret);

  std::vector<int> visited;
  std::vector<ir::IrBlock const*> path;
  std::vector<std::pair<ir::IrBlock const*, int>> proc;
  visited.resize(routine._blocks.size(), kUnvisited);
  proc.emplace_back(&routine._blocks[routine._root_id], 1);

  while (!proc.empty()) {
    auto [block, depth] = proc.back();
    proc.pop_back();
    path.resize(depth);
    path.back() = block;

    for (ir::BlockTransition const& edge : block->_tr_out) {
      if (visited[edge._target_idx] == kUnvisited) {
        proc.emplace_back(&routine._blocks[edge._target_idx], depth + 1);
      } else if (visited[edge._target_idx] == kNotLoopHeader) {
        visited[edge._target_idx] = ret._loops.size();
        Loop& new_loop = ret._loops.emplace_back();
        new_loop._header = &routine._blocks[edge._target_idx];
        new_loop._backedge.push_back(&edge);
      } else {
        ret._loops[visited[edge._target_idx]]._backedge.push_back(&edge);
      }
    }
  }

  return ret;
}
}  // namespace

void run_control_flow_analysis(ir::IrRoutine const& routine) {}
}  // namespace decomp::hll
