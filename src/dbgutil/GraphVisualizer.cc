#include "dbgutil/GraphVisualizer.hh"

#include <unordered_set>
#include <queue>

namespace decomp::vis {
namespace {
std::unordered_set<BasicBlock*> future_set(BasicBlock* node) {
  std::unordered_set<BasicBlock*> ret;

  std::vector<BasicBlock*> to_process;
  to_process.push_back(node);

  while (!to_process.empty()) {
    BasicBlock* cur = to_process.back();
    to_process.pop_back();

    if (ret.count(cur) > 0) {
      continue;
    }
    ret.emplace(cur);
    to_process.insert(to_process.end(), cur->outgoing_edges.begin(), cur->outgoing_edges.end());
  }

  return ret;
}
}  // namespace

VerticalGraph visualize_vertical(SubroutineGraph const& graph) {
  VerticalGraph ret;

  return ret;
}

}  // namespace decomp::vis
