#pragma once

#include <cstdint>
#include <vector>
#include <tuple>
#include <unordered_set>

namespace decomp {
class RandomAccessData;

enum class EdgeType {
  kUnconditional,
  kConditionTrue,
  kConditionFalse,
  kFallthrough,
};

struct BasicBlock {
  // Inclusive start address
  uint32_t block_start;
  // Exclusive end address
  uint32_t block_end;

  // TODO: Add generic extension data to tag onto blocks
  std::vector<BasicBlock*> incoming_edges;
  std::vector<std::tuple<EdgeType, BasicBlock*>> outgoing_edges;
};

struct Loop {
  BasicBlock* loop_start;
  std::vector<BasicBlock*> loop_exits;
  
  Loop(BasicBlock* start) : loop_start(start), loop_exits() {}
};

struct SubroutineGraph {
  BasicBlock* root;
  std::vector<BasicBlock*> exit_points;
  std::vector<Loop> loops;
};

SubroutineGraph create_graph(RandomAccessData const& ram, uint32_t subroutine_start);
std::unordered_set<BasicBlock*> future_set(BasicBlock* node);
}  // namespace decomp
