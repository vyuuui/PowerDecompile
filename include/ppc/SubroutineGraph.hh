#pragma once

#include <cstdint>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "ppc/Perilogue.hh"
#include "ppc/PpcDisasm.hh"
#include "ppc/RegisterLiveness.hh"
#include "producers/RandomAccessData.hh"
#include "utl/FlowGraph.hh"
#include "utl/IntervalTree.hh"

namespace decomp::ppc {
struct BasicBlock {
  BasicBlock(uint32_t start_va, uint32_t end_va) : _block_start(start_va), _block_end(end_va) {}
  // Inclusive start address
  uint32_t _block_start;
  // Exclusive end address
  uint32_t _block_end;

  std::vector<MetaInst> _instructions;

  std::unique_ptr<GprLiveness> _gpr_lifetimes;
  std::unique_ptr<FprLiveness> _fpr_lifetimes;
  std::unique_ptr<CrLiveness> _cr_lifetimes;
  std::vector<PerilogueInstructionType> _perilogue_types;
};
using BasicBlockVertex = FlowVertex<BasicBlock>;

class SubroutineGraph : public FlowGraph<BasicBlock> {
public:
  dinterval_tree<int, uint32_t> _nodes_by_range;
  std::vector<uint32_t> _direct_calls;

  BasicBlock const* block_by_vaddr(uint32_t vaddr) const {
    auto result = _nodes_by_range.query(vaddr, vaddr + 4);
    return result == nullptr ? nullptr : std::get_if<BasicBlock>(&vertex(*result)->_d);
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

void run_graph_analysis(Subroutine& routine, BinaryContext const& ctx, uint32_t subroutine_start);
}  // namespace decomp::ppc
