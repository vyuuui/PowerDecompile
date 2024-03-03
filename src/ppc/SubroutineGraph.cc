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
BasicBlockVertex* at_block_head(SubroutineGraph* graph, uint32_t address) {
  return graph->find_real([address](BasicBlockVertex& vert) { return address == vert.data()._block_start; });
}

BasicBlockVertex* contained_in_block(SubroutineGraph* graph, uint32_t address) {
  return graph->find_real([address](BasicBlockVertex& vert) {
    return address > vert.data()._block_start && address < vert.data()._block_end;
  });
}

BasicBlockVertex* split_blocks(BasicBlockVertex* original_block, uint32_t address, SubroutineGraph& graph) {
  // Original block is on top
  int new_block = graph.insert_after(
    original_block, BasicBlock(address, original_block->data()._block_end), BlockTransfer::kFallthrough);
  original_block->data()._block_end = address;

  return graph.vertex(new_block);
}
}  // namespace

void run_graph_analysis(Subroutine& routine, BinaryContext const& ctx, uint32_t subroutine_start) {
  RandomAccessData const& ram = *ctx._ram;

  std::unique_ptr<SubroutineGraph> graph = std::make_unique<SubroutineGraph>();
  BasicBlockVertex* start = graph->vertex(graph->emplace_vertex(subroutine_start, subroutine_start + 4));

  graph->emplace_link(graph->root(), start, BlockTransfer::kFallthrough);

  std::vector<BasicBlockVertex*> known_blocks;
  std::vector<BasicBlockVertex*> block_stack;

  auto handle_branch =
    [&block_stack, &graph](
      BasicBlockVertex* cur_block, uint32_t target_addr, uint32_t inst_addr, BlockTransfer branch_type) {
      if (BasicBlockVertex* known_block = at_block_head(graph.get(), target_addr); known_block != nullptr) {
        // If we're branching into the start of another block, just link us.
        graph->emplace_link(cur_block, known_block, branch_type);
        return;
      }

      BasicBlockVertex* next_block = nullptr;
      if (BasicBlockVertex* known_block = contained_in_block(graph.get(), target_addr); known_block != nullptr) {
        next_block = split_blocks(known_block, target_addr, *graph);
      } else {
        next_block = graph->vertex(graph->emplace_vertex(target_addr, target_addr + 4));

        block_stack.push_back(next_block);
      }

      graph->emplace_link(cur_block, next_block, branch_type);
      cur_block->data()._block_end = inst_addr + 0x4;
    };

  block_stack.push_back(start);

  // Build initial graph
  while (!block_stack.empty()) {
    BasicBlockVertex* this_block = block_stack.back();
    block_stack.pop_back();

    for (uint32_t inst_address = this_block->data()._block_start;; inst_address += 0x4) {
      // Extend the current block
      this_block->data()._block_end = inst_address + 0x4;

      // Check if we're falling through to an already known block
      if (BasicBlockVertex* fallthrough_block = at_block_head(graph.get(), inst_address);
          fallthrough_block != nullptr && fallthrough_block != this_block) {
        graph->emplace_link(this_block, fallthrough_block, BlockTransfer::kFallthrough);
        this_block->data()._block_end = inst_address;
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
          break;

        case InstOperation::kB: {
          const bool absolute = check_flags(inst._flags, InstFlags::kAbsoluteAddr);
          const uint32_t target_off = static_cast<uint32_t>(std::get<RelBranch>(inst._reads[0])._rel_32);
          const uint32_t target_addr = absolute ? target_off : inst_address + target_off;

          handle_branch(this_block, target_addr, inst_address, BlockTransfer::kUnconditional);

          break;
        }

        case InstOperation::kBc: {
          const bool absolute = check_flags(inst._flags, InstFlags::kAbsoluteAddr);
          const uint32_t target_off = static_cast<uint32_t>(std::get<RelBranch>(inst._reads[2])._rel_32);
          const uint32_t target_addr = absolute ? target_off : inst_address + target_off;
          const uint32_t next_addr = inst_address + 0x4;

          handle_branch(this_block, target_addr, inst_address, BlockTransfer::kConditionTrue);
          handle_branch(this_block, next_addr, inst_address, BlockTransfer::kConditionFalse);

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

  // Fill in instructions and interval tree
  graph->foreach_real([&graph, &ram](BasicBlockVertex& bbv) {
    for (uint32_t addr = bbv.data()._block_start; addr < bbv.data()._block_end; addr += 4) {
      bbv.data()._instructions.emplace_back(ram.read_instruction(addr));
    }

    graph->_nodes_by_range.try_emplace(bbv.data()._block_start, bbv.data()._block_end, bbv._idx);

    if (bbv._out.empty()) {
      graph->emplace_link(&bbv, graph->terminal(), BlockTransfer::kUnconditional);
    }
  });

  routine._graph = std::move(graph);
}

}  // namespace decomp::ppc
