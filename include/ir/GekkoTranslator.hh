#pragma once

#include <vector>

#include "ir/IrInst.hh"
#include "ir/RegisterBinding.hh"
#include "ppc/Subroutine.hh"

namespace decomp::ir {
struct BlockTransition {
  uint32_t _target_idx;
  uint32_t _checked_bits;
  uint32_t _invert_mask;
};

struct IrBlock {
  std::vector<IrInst> _insts;
  std::vector<BlockTransition> _tr_out;
  std::vector<BlockTransition> _tr_in;
};

struct IrGraph {
  std::vector<IrBlock> _blocks;
  GPRBindTracker _gpr_binds;

  IrGraph(ppc::Subroutine const& routine) : _gpr_binds(routine) { _blocks.resize(routine._graph._nodes_by_id.size()); }
};

IrGraph translate_subroutine(ppc::Subroutine const& routine);
}  // namespace decomp::ir
