#include "hll/Structurizer.hh"

#include <limits>
#include <numeric>
#include <set>
#include <vector>

namespace decomp::hll {
namespace {
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

template <bool PreDominator>
std::vector<int> lengauer_tarjan(ir::IrRoutine const& routine) {
  const int nblks = routine._graph.size();
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

  int dfs_num = 0;
  routine._graph.preorder_pathacc<PreDominator, int>(
    [&dfs_num, &sdom, &dfs2vert, &parent](ir::IrBlockVertex const& block, int dfsparent) {
      sdom[block._idx] = dfs_num;
      dfs2vert[dfs_num] = block._idx;
      parent[block._idx] = dfsparent;
      return block._idx;
    },
    PreDominator ? routine._graph.root() : routine._graph.terminal(),
    routine._graph.root()->_idx);

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
    ir::IrBlockVertex const* w_blk = routine._graph.vertex(w_gr);

    for (auto [v_gr, _] : w_blk->_in) {
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

  return idom;
}
}  // namespace

void ControlFlowStructurizer::prepare(ir::IrRoutine const& routine) {
  _routine = &routine;
  _idom_tree = lengauer_tarjan<true>(routine);
  _ipdom_tree = lengauer_tarjan<false>(routine);
  for (ir::IrBlockVertex const& block : routine._graph) {
    _leaves.push_back(new BasicBlock(block));
  }
}

HLLControlTree run_control_flow_analysis(ControlFlowStructurizer* structurizer, ir::IrRoutine const& routine) {
  structurizer->prepare(routine);
  return structurizer->structurize();
}
}  // namespace decomp::hll
