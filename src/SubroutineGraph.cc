#include "SubroutineGraph.hh"

#include <algorithm>
#include <stack>
#include <tuple>

#include "producers/RandomAccessData.hh"

namespace decomp {
namespace {

struct pair_hash {
  constexpr std::size_t operator()(std::pair<uint32_t, uint32_t> const& v) const { return v.first ^ v.second; }
};
using cut_set = std::unordered_set<std::pair<uint32_t, uint32_t>, pair_hash>;

///////////////////////////////
// Loop construction helpers //
///////////////////////////////
// Construct a loop provided its starting node and a list of cut edges to ignore. This should fill out the loop_contents
// and loop_exits fields
void construct_loop(Loop& loop, cut_set const& cuts) {
  std::vector<BasicBlock*> traversal_path;
  loop.loop_contents.emplace(loop.loop_start);

  dfs_forward(
      // Visit
      [&traversal_path, &loop](BasicBlock* cur, int prefix_depth) {
        traversal_path.resize(prefix_depth);
        traversal_path.push_back(cur);

        for (auto&& [_, next] : cur->outgoing_edges) {
          if (loop.loop_contents.count(next) > 0) {
            loop.loop_contents.insert(traversal_path.begin(), traversal_path.end());
            break;
          }
        }
      },
      // Iterate
      [&cuts](BasicBlock* next, BasicBlock* cur, int depth) -> std::optional<std::tuple<int>> {
        if (cuts.count(std::pair<int, int>(cur->block_id, next->block_id)) > 0) {
          return std::nullopt;
        }
        return std::tuple<int>(depth + 1);
      },
      loop.loop_start,
      0);

  for (BasicBlock* loop_block : loop.loop_contents) {
    for (auto&& [_, node] : loop_block->outgoing_edges) {
      if (loop.loop_contents.count(node) == 0) {
        loop.loop_exits.emplace_back(node);
      }
    }
  }
}

// Returns the future set, ignoring cuts made on edges
std::unordered_set<BasicBlock*> future_set_with_cuts(BasicBlock* node, cut_set const& cuts) {
  return dfs_forward([](BasicBlock*) {},
      [&cuts](BasicBlock* next, BasicBlock* cur) {
        return cuts.count(std::pair<int, int>(cur->block_id, next->block_id)) == 0;
      },
      node);
}

////////////////////////////////
// Graph construction helpers //
////////////////////////////////
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

BasicBlock* split_blocks(BasicBlock* original_block, uint32_t address, SubroutineGraph& graph) {
  BasicBlock* new_block = new BasicBlock();
  new_block->block_start = address;
  new_block->block_end = original_block->block_end;
  new_block->incoming_edges.push_back({IncomingEdgeType::kForwardEdge, original_block});
  new_block->block_id = static_cast<uint32_t>(graph.nodes_by_id.size());
  graph.nodes_by_id.push_back(new_block);

  // Original block is on top
  new_block->outgoing_edges = std::move(original_block->outgoing_edges);
  original_block->outgoing_edges.push_back({OutgoingEdgeType::kFallthrough, new_block});
  original_block->block_end = address;

  return new_block;
}

}  // namespace

SubroutineGraph create_graph(RandomAccessData const& ram, uint32_t subroutine_start) {
  SubroutineGraph graph;
  BasicBlock* start = new BasicBlock();
  start->block_start = subroutine_start;
  start->block_id = 0;

  graph.root = start;
  graph.nodes_by_id.push_back(start);

  std::vector<BasicBlock*> known_blocks;
  std::vector<BasicBlock*> block_stack;

  auto handle_branch =
      [&known_blocks, &block_stack, &graph](
          BasicBlock* cur_block, uint32_t target_addr, uint32_t inst_addr, OutgoingEdgeType branch_type) {
        if (BasicBlock* known_block = at_block_head(known_blocks, target_addr); known_block != nullptr) {
          // If we're branching into the start of another block, just link us.
          known_block->incoming_edges.push_back({IncomingEdgeType::kForwardEdge, cur_block});
          cur_block->outgoing_edges.push_back({branch_type, known_block});
          return;
        }

        BasicBlock* next_block = nullptr;
        if (BasicBlock* known_block = contained_in_block(known_blocks, target_addr); known_block != nullptr) {
          next_block = split_blocks(known_block, target_addr, graph);
        } else {
          next_block = new BasicBlock();
          next_block->block_start = target_addr;
          next_block->block_end = target_addr + 4;
          next_block->block_id = static_cast<uint32_t>(graph.nodes_by_id.size());
          graph.nodes_by_id.push_back(next_block);

          block_stack.push_back(next_block);
          known_blocks.push_back(next_block);
        }

        next_block->incoming_edges.push_back({IncomingEdgeType::kForwardEdge, cur_block});
        cur_block->outgoing_edges.push_back({branch_type, next_block});
        cur_block->block_end = inst_addr + 0x4;
      };

  block_stack.push_back(start);
  known_blocks.push_back(start);

  // Build initial graph
  while (!block_stack.empty()) {
    BasicBlock* this_block = block_stack.back();
    block_stack.pop_back();

    for (uint32_t inst_address = this_block->block_start;; inst_address += 0x4) {
      // Extend the current block
      this_block->block_end = inst_address + 0x4;
      // Check if we're falling through to an already known block
      auto fallthrough_block = at_block_head(known_blocks, inst_address);
      if (fallthrough_block != nullptr && fallthrough_block != this_block) {
        this_block->outgoing_edges.push_back({OutgoingEdgeType::kFallthrough, fallthrough_block});
        this_block->block_end = inst_address;
        fallthrough_block->incoming_edges.push_back({IncomingEdgeType::kForwardEdge, this_block});
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
          const uint32_t target_off = static_cast<uint32_t>(std::get<RelBranch>(inst._immediates[0])._rel_32);
          const uint32_t target_addr = absolute ? target_off : inst_address + target_off;

          handle_branch(this_block, target_addr, inst_address, OutgoingEdgeType::kUnconditional);

          break;
        }

        case InstOperation::kBc: {
          const bool absolute = check_flags(inst._flags, InstFlags::kAbsoluteAddr);
          const uint32_t target_off = static_cast<uint32_t>(std::get<RelBranch>(inst._immediates[1])._rel_32);
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

  // Fill range tree with node ranges
  dfs_forward([&graph](BasicBlock* cur) { graph.nodes_by_range.try_emplace(cur->block_start, cur->block_end, cur); },
      [](BasicBlock*, BasicBlock*) { return true; },
      graph.root);

  // Loop detection algorithm
  //   For each node in the graph
  //   If the node has at least one in edge not in its future set and at least one in edge in its future set
  //   + Construct a loop starting at this node
  //   + Make cuts in the graph for each edge that is in the future set of this node
  cut_set cuts;
  dfs_forward(
      // Visit
      [&graph, &cuts](BasicBlock* cur) {
        if (cur->incoming_edges.empty()) {
          return;
        }

        std::unordered_set<BasicBlock*> future = future_set_with_cuts(cur, cuts);
        bool incoming_in_future = false;
        bool incoming_outside_loop = false;

        for (auto&& [_, node] : cur->incoming_edges) {
          if (future.count(node) > 0) {
            incoming_in_future = true;
          } else {
            incoming_outside_loop = true;
          }
        }

        if (incoming_in_future && incoming_outside_loop) {
          construct_loop(graph.loops.emplace_back(cur), cuts);
          for (auto&& [edge_type, node] : cur->incoming_edges) {
            if (future.count(node) > 0) {
              // All edges that point back to the loop entry point are backedges
              edge_type = IncomingEdgeType::kBackEdge;
              // Cut all backedges to this newly created loop
              cuts.emplace(node->block_id, cur->block_id);
            }
          }
        }
      },
      // Iterate
      [](BasicBlock*, BasicBlock*) { return true; },
      graph.root);

  return graph;
}

}  // namespace decomp
