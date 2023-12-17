#pragma once

#include <cstdint>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "ppc/Perilogue.hh"
#include "ppc/PpcDisasm.hh"
#include "ppc/RegisterLiveness.hh"
#include "producers/RandomAccessData.hh"
#include "utl/IntervalTree.hh"

namespace decomp::ppc {
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

struct BasicBlock {
  // Inclusive start address
  uint32_t _block_start;
  // Exclusive end address
  uint32_t _block_end;
  uint32_t _block_id;

  bool _terminal_block;

  std::vector<std::tuple<IncomingEdgeType, BasicBlock*>> _incoming_edges;
  std::vector<std::tuple<OutgoingEdgeType, BasicBlock*>> _outgoing_edges;

  std::vector<MetaInst> _instructions;

  std::unique_ptr<GprLiveness> _gpr_lifetimes;
  std::unique_ptr<FprLiveness> _fpr_lifetimes;
  std::unique_ptr<CrLiveness> _cr_lifetimes;
  std::vector<PerilogueInstructionType> _perilogue_types;
};

struct SubroutineGraph {
  BasicBlock* _root;
  std::vector<BasicBlock*> _nodes_by_id;
  dinterval_tree<BasicBlock*, uint32_t> _nodes_by_range;
  std::vector<BasicBlock*> _exit_points;
  std::vector<uint32_t> _direct_calls;

  BasicBlock const* block_by_vaddr(uint32_t vaddr) const {
    auto result = _nodes_by_range.query(vaddr, vaddr + 4);
    return result == nullptr ? nullptr : *result;
  }
  BasicBlock* block_by_vaddr(uint32_t vaddr) {
    return const_cast<BasicBlock*>(const_cast<SubroutineGraph const*>(this)->block_by_vaddr(vaddr));
  }
};

template <typename SetType>
constexpr RegisterLiveness<typename SetType::RegType> const* get_liveness(BasicBlock const* block) {
  if constexpr (std::is_same_v<SetType, GprSet>) {
    return block->_gpr_lifetimes.get();
  } else if constexpr (std::is_same_v<SetType, FprSet>) {
    return block->_fpr_lifetimes.get();
  } else if constexpr (std::is_same_v<SetType, CrSet>) {
    return block->_cr_lifetimes.get();
  }
}

template <typename SetType>
constexpr RegisterLiveness<typename SetType::RegType>* get_liveness(BasicBlock* block) {
  if constexpr (std::is_same_v<SetType, GprSet>) {
    return block->_gpr_lifetimes.get();
  } else if constexpr (std::is_same_v<SetType, FprSet>) {
    return block->_fpr_lifetimes.get();
  } else if constexpr (std::is_same_v<SetType, CrSet>) {
    return block->_cr_lifetimes.get();
  }
}

template <bool Forward, typename Visit, typename Iterate, typename... Annotation>
void dfs_traversal(Visit&& visitor, Iterate&& iterator, BasicBlock* start, Annotation... init_annot) {
  using EdgeListType =
    std::conditional_t<Forward, decltype(BasicBlock::_outgoing_edges), decltype(BasicBlock::_incoming_edges)>;

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
      edge_list = &std::get<0>(tup)->_outgoing_edges;
    } else {
      edge_list = &std::get<0>(tup)->_incoming_edges;
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
}

template <typename Visit, typename Iterate, typename... Annotation>
void dfs_forward(Visit&& visitor, Iterate&& iterator, BasicBlock* start, Annotation... init_annot) {
  dfs_traversal<true>(
    std::forward<Visit>(visitor), std::forward<Iterate>(iterator), start, std::forward<Annotation>(init_annot)...);
}
template <typename Visit, typename Iterate, typename... Annotation>
void dfs_backward(Visit&& visitor, Iterate&& iterator, BasicBlock* start, Annotation... init_annot) {
  dfs_traversal<false>(
    std::forward<Visit>(visitor), std::forward<Iterate>(iterator), start, std::forward<Annotation>(init_annot)...);
}

constexpr bool always_iterate(BasicBlock const*, BasicBlock const*) { return true; }

void run_graph_analysis(Subroutine& routine, BinaryContext const& ctx, uint32_t subroutine_start);
}  // namespace decomp::ppc
