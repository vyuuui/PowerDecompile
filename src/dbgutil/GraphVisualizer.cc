#include "dbgutil/GraphVisualizer.hh"

#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace decomp::vis {

VerticalGraph visualize_vertical(SubroutineGraph const& graph) {
  VerticalGraph ret;

  std::unordered_set<BasicBlock*> visited;
  std::queue<size_t> pq;
  std::vector<BlockCell> cell_pool;
  std::unordered_map<BasicBlock*, size_t> cell_map;
  // std::unordered_set<BlockCell> block_rows;

  cell_pool.emplace_back(graph.root, 0, 0);
  pq.emplace(0);
  cell_map.emplace(graph.root, 0);

  while (!pq.empty()) {
    size_t current_idx = pq.front();
    BlockCell current = cell_pool[current_idx];
    pq.pop();

    if (visited.count(current._basic_block) > 0) {
      continue;
    }

    auto future_blocks = future_set(current._basic_block);

    bool defer = false;
    for (BasicBlock* incoming : current._basic_block->incoming_edges) {
      if (visited.count(incoming) == 0 && future_blocks.count(incoming) == 0) {
        pq.emplace(current_idx);
        defer = true;
        break;
      }
    }

    if (defer) {
      continue;
    }

    visited.emplace(current._basic_block);

    if (ret._rows.size() <= current._row) {
      ret._rows.emplace_back();
    }

    ret._rows[current._row].emplace_back(
        current._basic_block->block_start,
        current._basic_block->block_end - current._basic_block->block_start);

    for (uint32_t i = 0; i < current._basic_block->outgoing_edges.size(); i++) {
      auto pair = current._basic_block->outgoing_edges[i];
      BasicBlock* edge_block = std::get<1>(pair);
      auto it = cell_map.find(edge_block);
      if (it != cell_map.end()) {
        cell_pool[it->second]._row = current._row + 1;
      } else {
        pq.emplace(cell_pool.size());
        cell_map.emplace(edge_block, cell_pool.size());
        cell_pool.emplace_back(edge_block, current._row + 1 /* row */, 0 /* column */);
      }
    }
  }

  return ret;
}
}  // namespace decomp::vis
