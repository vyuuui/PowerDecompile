#include "SubroutineGraph.hh"

#include <algorithm>
#include <stack>
#include <tuple>

#include "producers/RandomAccessData.hh"

namespace decomp {
namespace {

BasicBlock* at_block_head(std::vector<BasicBlock*>& blocks, uint32_t address) {
  for (BasicBlock* block : blocks) {
    if (address == block->block_start) {
      return block;
    }
  }

  return nullptr;
}

BasicBlock* contained_in_block(std::vector<BasicBlock*>& blocks, uint32_t address) {
  for (BasicBlock* block : blocks) {
    if (address > block->block_start && address < block->block_end) {
      return block;
    }
  }

  return nullptr;
}

BasicBlock* split_blocks(BasicBlock* original_block, uint32_t address) {
  BasicBlock* new_block = new BasicBlock();
  new_block->block_start = address;
  new_block->block_end = original_block->block_end;
  new_block->incoming_edges.push_back(original_block);

  // Original block is on top
  new_block->outgoing_edges = std::move(original_block->outgoing_edges);
  original_block->outgoing_edges.push_back({EdgeType::kFallthrough, new_block});
  original_block->block_end = address;

  return new_block;
}
}  // namespace

std::unordered_set<BasicBlock*> future_set(BasicBlock* node) {
  std::unordered_set<BasicBlock*> ret;

  std::vector<std::tuple<EdgeType, BasicBlock*>> to_process;
  to_process.push_back({EdgeType::kUnconditional, node});

  while (!to_process.empty()) {
    BasicBlock* cur = std::get<1>(to_process.back());
    to_process.pop_back();

    if (ret.count(cur) > 0) {
      continue;
    }
    ret.emplace(cur);
    to_process.insert(to_process.end(), cur->outgoing_edges.begin(), cur->outgoing_edges.end());
  }

  return ret;
}

// 88 miles per hour
std::unordered_set<BasicBlock*> past_set(BasicBlock* node) {
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
    to_process.insert(to_process.end(), cur->incoming_edges.begin(), cur->incoming_edges.end());
  }

  return ret;
}

SubroutineGraph create_graph(RandomAccessData const& ram, uint32_t subroutine_start) {
  BasicBlock* start = new BasicBlock();
  start->block_start = subroutine_start;

  SubroutineGraph graph;
  graph.root = start;

  std::vector<BasicBlock*> known_blocks;
  std::vector<BasicBlock*> block_stack;

  auto handle_branch = [&known_blocks, &block_stack](BasicBlock* cur_block, uint32_t target_addr,
                                                     uint32_t inst_addr, EdgeType branch_type) {
    if (BasicBlock* known_block = at_block_head(known_blocks, target_addr);
        known_block != nullptr) {
      // If we're branching into the start of another block, just link us.
      known_block->incoming_edges.push_back(cur_block);
      cur_block->outgoing_edges.push_back({branch_type, known_block});
      return;
    }

    BasicBlock* next_block = nullptr;
    if (BasicBlock* known_block = contained_in_block(known_blocks, target_addr);
        known_block != nullptr) {
      next_block = split_blocks(known_block, target_addr);
    } else {
      next_block = new BasicBlock();
      next_block->block_start = target_addr;
      next_block->block_end = target_addr + 4;

      block_stack.push_back(next_block);
      known_blocks.push_back(next_block);
    }

    next_block->incoming_edges.push_back(cur_block);
    cur_block->outgoing_edges.push_back({branch_type, next_block});
    cur_block->block_end = inst_addr + 0x4;
  };

  block_stack.push_back(start);
  known_blocks.push_back(start);

  while (!block_stack.empty()) {
    BasicBlock* this_block = block_stack.back();
    block_stack.pop_back();

    for (uint32_t inst_address = this_block->block_start;; inst_address += 0x4) {
      // Extend the current block
      this_block->block_end = inst_address + 0x4;
      // Check if we're falling through to an already known block
      auto fallthrough_block = at_block_head(known_blocks, inst_address);
      if (fallthrough_block != nullptr && fallthrough_block != this_block) {
        this_block->outgoing_edges.push_back({EdgeType::kFallthrough, fallthrough_block});
        this_block->block_end = inst_address;
        fallthrough_block->incoming_edges.push_back(this_block);
        break;
      }

      MetaInst inst = ram.read_instruction(inst_address);
      // The node isn't over if we're returning after the branch.
      if (check_flags(inst._flags, InstFlags::kWritesLR)) {
        continue;
      }

      // Are we at the end and need a new block?
      bool terminate_block = true;
      switch (inst._op) {
        case InstOperation::kBclr:
        case InstOperation::kBcctr:
          graph.exit_points.push_back(this_block);
          break;

        case InstOperation::kB: {
          const bool absolute = check_flags(inst._flags, InstFlags::kAbsoluteAddr);
          const uint32_t target_off =
              static_cast<uint32_t>(std::get<RelBranch>(inst._immediates[0])._rel_32);
          const uint32_t target_addr = absolute ? target_off : inst_address + target_off;

          handle_branch(this_block, target_addr, inst_address, EdgeType::kUnconditional);

          break;
        }

        case InstOperation::kBc: {
          const bool absolute = check_flags(inst._flags, InstFlags::kAbsoluteAddr);
          const uint32_t target_off =
              static_cast<uint32_t>(std::get<RelBranch>(inst._immediates[1])._rel_32);
          const uint32_t target_addr = absolute ? target_off : inst_address + target_off;
          const uint32_t next_addr = inst_address + 0x4;

          handle_branch(this_block, target_addr, inst_address, EdgeType::kConditionTrue);
          handle_branch(this_block, next_addr, inst_address, EdgeType::kConditionFalse);

          break;
        }

        default:
          terminate_block = false;
          break;
      }

      if (terminate_block) {
        break;
      }
    }
  }

  for (BasicBlock* this_block : future_set(graph.root)) {
    // Check if we're in a loop.
    if (!this_block->incoming_edges.empty()) {
      std::unordered_set<BasicBlock*> future = future_set(this_block);

      // We determine if this counts as the start of a loop by if it is entered by something
      // outside of the loop and is indeed part of a loop.
      bool incoming_outside_loop = false;
      bool incoming_in_future = false;
      for (auto& incoming : this_block->incoming_edges) {
        if (future.count(incoming) > 0) {
          incoming_in_future = true;
        } else {
          incoming_outside_loop = true;
        }
      }

      if (!incoming_outside_loop && incoming_in_future) {
        graph.loops.emplace_back(this_block);
      }
    }

    if (!graph.loops.empty()) {
      for (auto& outgoing : this_block->outgoing_edges) {
        BasicBlock* edge = std::get<1>(outgoing);

        // This is a branch back up, and therefore can't be an exit.
        if (edge->block_start < this_block->block_start) continue;

        std::unordered_set<BasicBlock*> future = future_set(edge);
        for (auto& loop : graph.loops) {
          // This is above the loop start, so it can't be an exit.
          if (edge->block_start < loop.loop_start->block_start) {
            continue;
          }

          if (future.count(loop.loop_start) == 0) {
            loop.loop_exits.emplace_back(edge);
          }
        }
      }
    }
  }

  for (auto loop : graph.loops) {
    assert(!loop.loop_exits.empty());
  }

  return graph;
}

}  // namespace decomp
