#include "ppc/SubroutineGraph.hh"

#include <algorithm>
#include <stack>
#include <tuple>

#include "producers/RandomAccessData.hh"

namespace decomp::ppc {
namespace {
////////////////////////////////
// Graph construction helpers //
////////////////////////////////
BasicBlock* at_block_head(std::vector<BasicBlock*>& blocks, uint32_t address) {
  for (BasicBlock* block : blocks) {
    if (address == block->_block_start) {
      return block;
    }
  }

  return nullptr;
}

BasicBlock* contained_in_block(std::vector<BasicBlock*>& blocks, uint32_t address) {
  for (BasicBlock* block : blocks) {
    if (address > block->_block_start && address < block->_block_end) {
      return block;
    }
  }

  return nullptr;
}

BasicBlock* split_blocks(BasicBlock* original_block, uint32_t address, SubroutineGraph& graph) {
  BasicBlock* new_block = new BasicBlock();
  new_block->_block_start = address;
  new_block->_block_end = original_block->_block_end;
  new_block->_block_id = static_cast<uint32_t>(graph._nodes_by_id.size());
  new_block->_terminal_block = original_block->_terminal_block;
  graph._nodes_by_id.push_back(new_block);

  // Original block is on top
  new_block->_outgoing_edges = std::move(original_block->_outgoing_edges);
  original_block->_outgoing_edges.push_back({OutgoingEdgeType::kFallthrough, new_block});
  original_block->_block_end = address;
  original_block->_terminal_block = false;

  return new_block;
}
}  // namespace

void run_graph_analysis(Subroutine& routine, BinaryContext const& ctx, uint32_t subroutine_start) {
  RandomAccessData const& ram = *ctx._ram;

  std::unique_ptr<SubroutineGraph> graph = std::make_unique<SubroutineGraph>();
  BasicBlock* start = new BasicBlock();
  start->_block_start = subroutine_start;
  start->_block_id = 0;

  graph->_root = start;
  graph->_nodes_by_id.push_back(start);

  std::vector<BasicBlock*> known_blocks;
  std::vector<BasicBlock*> block_stack;

  auto handle_branch =
    [&known_blocks, &block_stack, &graph](
      BasicBlock* cur_block, uint32_t target_addr, uint32_t inst_addr, OutgoingEdgeType branch_type) {
      if (BasicBlock* known_block = at_block_head(known_blocks, target_addr); known_block != nullptr) {
        // If we're branching into the start of another block, just link us.
        cur_block->_outgoing_edges.push_back({branch_type, known_block});
        return;
      }

      BasicBlock* next_block = nullptr;
      if (BasicBlock* known_block = contained_in_block(known_blocks, target_addr); known_block != nullptr) {
        next_block = split_blocks(known_block, target_addr, *graph);
      } else {
        next_block = new BasicBlock();
        next_block->_block_start = target_addr;
        next_block->_block_end = target_addr + 4;
        next_block->_block_id = static_cast<uint32_t>(graph->_nodes_by_id.size());
        graph->_nodes_by_id.push_back(next_block);

        block_stack.push_back(next_block);
        known_blocks.push_back(next_block);
      }

      cur_block->_outgoing_edges.push_back({branch_type, next_block});
      cur_block->_block_end = inst_addr + 0x4;
    };

  block_stack.push_back(start);
  known_blocks.push_back(start);

  // Build initial graph
  while (!block_stack.empty()) {
    BasicBlock* this_block = block_stack.back();
    block_stack.pop_back();

    for (uint32_t inst_address = this_block->_block_start;; inst_address += 0x4) {
      // Extend the current block
      this_block->_block_end = inst_address + 0x4;
      // Check if we're falling through to an already known block
      auto fallthrough_block = at_block_head(known_blocks, inst_address);
      if (fallthrough_block != nullptr && fallthrough_block != this_block) {
        this_block->_outgoing_edges.push_back({OutgoingEdgeType::kFallthrough, fallthrough_block});
        this_block->_block_end = inst_address;
        break;
      }

      MetaInst inst = ram.read_instruction(inst_address);
      // The node isn't over if we're returning after the branch.
      if (check_flags(inst._flags, InstFlags::kWritesLR)) {
        if (inst._op == InstOperation::kB) {
          graph->_direct_calls.emplace_back(inst.branch_target());
        }
        continue;
      }

      // Are we at the end and need a new block?
      bool terminate_block = true;
      switch (inst._op) {
        case InstOperation::kBclr:
        case InstOperation::kBcctr:
          this_block->_terminal_block = true;
          break;

        case InstOperation::kB: {
          const bool absolute = check_flags(inst._flags, InstFlags::kAbsoluteAddr);
          const uint32_t target_off = static_cast<uint32_t>(std::get<RelBranch>(inst._reads[0])._rel_32);
          const uint32_t target_addr = absolute ? target_off : inst_address + target_off;

          handle_branch(this_block, target_addr, inst_address, OutgoingEdgeType::kUnconditional);

          break;
        }

        case InstOperation::kBc: {
          const bool absolute = check_flags(inst._flags, InstFlags::kAbsoluteAddr);
          const uint32_t target_off = static_cast<uint32_t>(std::get<RelBranch>(inst._reads[2])._rel_32);
          const uint32_t target_addr = absolute ? target_off : inst_address + target_off;
          const uint32_t next_addr = inst_address + 0x4;

          handle_branch(this_block, target_addr, inst_address, OutgoingEdgeType::kConditionTrue);
          handle_branch(this_block, next_addr, inst_address, OutgoingEdgeType::kConditionFalse);

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

  // Fill out block data now that blocks have been fully defined
  // TODO: Is there a way to not disassemble twice?
  dfs_forward(
    [&graph, &ram](BasicBlock* cur) {
      for (uint32_t addr = cur->_block_start; addr < cur->_block_end; addr += 4) {
        cur->_instructions.emplace_back(ram.read_instruction(addr));
      }
      graph->_nodes_by_range.try_emplace(cur->_block_start, cur->_block_end, cur);
    },
    [](BasicBlock* next, BasicBlock* cur) {
      next->_incoming_edges.emplace_back(IncomingEdgeType::kForwardEdge, cur);
      return true;
    },
    graph->_root);

  // Add all exit points at end
  for (BasicBlock* block : graph->_nodes_by_id) {
    if (block->_terminal_block) {
      graph->_exit_points.push_back(block);
    }
  }

  routine._graph = std::move(graph);
}

}  // namespace decomp::ppc
