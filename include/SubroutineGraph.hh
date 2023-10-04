#pragma once

#include <cstdint>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "PpcDisasm.hh"
#include "utl/IntervalTree.hh"

namespace decomp {
class RandomAccessData;

enum class IncomingEdgeType {
  kForwardEdge,
  kBackEdge,
};

enum class OutgoingEdgeType {
  kUnconditional,
  kConditionTrue,
  kConditionFalse,
  kFallthrough,
};

struct BlockPrivate {
  virtual ~BlockPrivate() {}
};

struct BasicBlock {
  // Inclusive start address
  uint32_t block_start;
  // Exclusive end address
  uint32_t block_end;
  uint32_t block_id;

  std::vector<std::tuple<IncomingEdgeType, BasicBlock*>> incoming_edges;
  std::vector<std::tuple<OutgoingEdgeType, BasicBlock*>> outgoing_edges;

  std::vector<MetaInst> instructions;

  BlockPrivate* extension_data = nullptr;
};

struct Loop {
  BasicBlock* loop_start;
  std::unordered_set<BasicBlock*> loop_contents;
  std::vector<BasicBlock*> loop_exits;

  Loop(BasicBlock* start) : loop_start(start), loop_exits() {}
};

struct SubroutineGraph {
  BasicBlock* root;
  std::vector<BasicBlock*> nodes_by_id;
  dinterval_tree<BasicBlock*, uint32_t> nodes_by_range;
  std::vector<BasicBlock*> exit_points;
  std::vector<Loop> loops;

  BasicBlock const* block_by_vaddr(uint32_t vaddr) const {
    auto result = nodes_by_range.query(vaddr, vaddr + 4);
    return result == nullptr ? nullptr : *result;
  }
  BasicBlock* block_by_vaddr(uint32_t vaddr) {
    return const_cast<BasicBlock*>(const_cast<SubroutineGraph const*>(this)->block_by_vaddr(vaddr));
  }
};

template <bool Forward, typename Visit, typename Iterate, typename... Annotation>
std::unordered_set<BasicBlock*> dfs_traversal(
    Visit&& visitor, Iterate&& iterator, BasicBlock* start, Annotation... init_annot) {
  using EdgeListType =
      std::conditional_t<Forward, decltype(BasicBlock::outgoing_edges), decltype(BasicBlock::incoming_edges)>;

  std::unordered_set<BasicBlock*> visited;
  std::vector<std::tuple<BasicBlock*, Annotation...>> process_stack;

  process_stack.emplace_back(start, std::forward<Annotation>(init_annot)...);

  while (!process_stack.empty()) {
    auto tup = std::move(process_stack.back());
    process_stack.pop_back();
    if (visited.count(std::get<0>(tup)) > 0) {
      continue;
    }
    visited.emplace(std::get<0>(tup));

    std::apply(visitor, tup);
    EdgeListType* edge_list;
    if constexpr (Forward) {
      edge_list = &std::get<0>(tup)->outgoing_edges;
    } else {
      edge_list = &std::get<0>(tup)->incoming_edges;
    }

    for (auto&& [_, next] : *edge_list) {
      auto ret = std::apply(iterator, std::tuple_cat(std::make_tuple(next), tup));
      if (ret) {
        if constexpr (sizeof...(Annotation) == 0) {
          process_stack.push_back(std::make_tuple(next));
        } else {
          process_stack.push_back(std::tuple_cat(std::make_tuple(next), *ret));
        }
      }
    }
  }

  return visited;
}

template <typename Visit, typename Iterate, typename... Annotation>
std::unordered_set<BasicBlock*> dfs_forward(
    Visit&& visitor, Iterate&& iterator, BasicBlock* start, Annotation... init_annot) {
  return dfs_traversal<true>(
      std::forward<Visit>(visitor), std::forward<Iterate>(iterator), start, std::forward<Annotation>(init_annot)...);
}
template <typename Visit, typename Iterate, typename... Annotation>
std::unordered_set<BasicBlock*> dfs_backward(
    Visit&& visitor, Iterate&& iterator, BasicBlock* start, Annotation... init_annot) {
  return dfs_traversal<false>(
      std::forward<Visit>(visitor), std::forward<Iterate>(iterator), start, std::forward<Annotation>(init_annot)...);
}

constexpr bool always_iterate(BasicBlock*, BasicBlock*) { return true; }

SubroutineGraph create_graph(RandomAccessData const& ram, uint32_t subroutine_start);

}  // namespace decomp
