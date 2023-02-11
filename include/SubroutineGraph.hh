#pragma once

#include <cstdint>
#include <vector>

namespace decomp {
class RandomAccessData;

struct BasicBlock {
  // Inclusive start address
  uint32_t block_start;
  // Exclusive end address
  uint32_t block_end;

  // TODO: Add generic extension data to tag onto blocks
  std::vector<BasicBlock*> incoming_edges;
  std::vector<BasicBlock*> outgoing_edges;
};

struct SubroutineGraph {
  BasicBlock* root;
  std::vector<BasicBlock*> exit_points;
};

SubroutineGraph create_graph(RandomAccessData const& ram, uint32_t subroutine_start);
}  // namespace decomp
