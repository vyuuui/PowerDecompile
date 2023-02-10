
#include "SubroutineGraph.hh"

#include <algorithm>
#include <stack>

#include "RandomAccessData.hh"

namespace decomp {

SubroutineGraph create_graph(RandomAccessData const& ram, uint32_t subroutine_start) {
  constexpr auto at_explored_block = [](std::vector<BasicBlock*>& blocks, uint32_t address) -> BasicBlock* {
    for (BasicBlock* block : blocks) {
      if (address == block->block_start) {
        return block;
      }
    }

    return nullptr;
  };
  
  constexpr auto are_in_explored = [](std::vector<BasicBlock*>& blocks, uint32_t address) -> BasicBlock* {
    for (BasicBlock* block : blocks) {
      if (address > block->block_start && address <= block->block_end) {
        return block;
      }
    }

    return nullptr;
  };

  constexpr auto split_blocks = [](BasicBlock* original_block, uint32_t address) -> BasicBlock* {
    BasicBlock* new_block = new BasicBlock();
    new_block->block_start = address;
    new_block->block_end = original_block->block_end;
    new_block->incoming_edges.push_back(original_block);
    
    // Original block is on top
    new_block->outgoing_edges = std::move(original_block->outgoing_edges);
    original_block->outgoing_edges.push_back(new_block);
    original_block->block_end = address - 0x4;

    return new_block;
  };

  constexpr auto handle_branch = [this](BasicBlock* current_block, uint32_t target_addr) -> void { 
      if (BasicBlock* explored_block = at_explored_block(explored, target_addr);
          explored_block != nullptr) {
        // If we're branching into the start of another block, just link us.
        explored_block->incoming_edges.push_back(current_block);
        current_block->outgoing_edges.push_back(explored_block);
        break;
      }

      BasicBlock* next_block = nullptr;
      if (BasicBlock* explored_block = are_inside_explored(explored, target_addr);
                 explored_block != nullptr) {
        next_block = split_blocks(explored_block, target_addr);
      } else {
        next_block = new BasicBlock();
        next_block->block_start = target_addr;
        
        block_stack.push_back(next_block);
      }

      next_block->incoming_edges.push_back(current_block);
      current_block->outgoing_edges.push_back(next_block);
      current_block->block_end = inst_address;
  };

  BasicBlock* start = new BasicBlock();
  start->block_start = subroutine_start;

  SubroutineGraph graph;
  graph.root = start;

  std::vector<BasicBlock*> explored;
  std::vector<BasicBlock*> block_stack;
  block_stack.push_back(start);

  while (!block_stack.empty()) {
    BasicBlock* this_block = block_stack.back();
    block_stack.pop_back();

    for (uint32_t inst_address = this_block->block_start; ; inst_address += 0x4) {
      // Check if we're falling through to an already processed block
      auto fallthrough_block = at_explored_block(explored, inst_address);
      if (fallthrough_block) {
        this_block->outgoing_edges.push_back(fallthrough_block);
        this_block->block_end = inst_address - 0x4;
        fallthrough_block->incoming_edges.push_back(this_block);
        break;
      }

      if (find_in_explored(explored, inst_address) == nullptr) {
        explored.push_back(this_block);
      }

      MetaInst inst = ram.read_instruction(inst_address);
      // The node isn't over if we're returning after the branch.
      if (check_flags(inst._flags, InstFlags::kWritesLR)) {
        continue;
      }

      // Are we at the end and need a new block?
      bool terminate_block = false;
      switch (inst._op) {
        case InstOperation::kBclr:
        case InstOperation::kBcctr:
          this_block->block_end = inst_address;
          graph.exit_points.push_back(this_block);
          terminate_block = true;
          break;

        case InstOperation::kB: {
          const bool absolute = check_flags(inst._flags, InstFlags::kAbsoluteAddr);
          const uint32_t target_off = static_cast<uint32_t>(std::get<RelBranch>(inst._immediates[0])._rel_32);
          const uint32_t target_addr = absolute ? target_off : inst_address + target_off;

          handle_branch(this_block, target_addr);
          terminate_block = true;

          break;
        }

        case InstOperation::kBc: {
          const bool absolute = check_flags(inst._flags, InstFlags::kAbsoluteAddr);
          const uint32_t target_off = static_cast<uint32_t>(std::get<RelBranch>(inst._immediates[1])._rel_32);
          const uint32_t target_addr = absolute ? target_off : inst_address + target_off;
          const uint32_t next_addr = inst_address + 0x4;

          handle_branch(this_block, target_addr); 

          BasicBlock* next_block = split_blocks(this_block, next_addr);
          next_block->incoming_edges.push_back(this_block);

          this_block->outgoing_edges.push_back(next_block);
          this_block->block_end(inst_address);
          
          block_stack.push_back(next_block);
          terminate_block = true;

          break;
        }

        default:
          break;
      }

      if (terminate_block) {
        break;
      }
    }
  }

  return graph;
}
}  // namespace decomp
