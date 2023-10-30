#pragma once

#include <vector>

#include "ir/IrInst.hh"
#include "ir/RegisterBinding.hh"
#include "ppc/Subroutine.hh"

namespace decomp::ir {
enum class CounterCheck {
  kIgnore,
  kCounterZero,
  kCounterNotZero,
};
struct BlockTransition {
  uint32_t _target_idx;
  uint32_t _checked_bits;
  uint32_t _invert_mask;
  bool _take_if_true;
  CounterCheck _counter;

  BlockTransition(uint32_t target_idx,
    uint32_t checked_bits,
    uint32_t invert_mask,
    bool take_if_true,
    CounterCheck counter = CounterCheck::kIgnore)
      : _target_idx(target_idx),
        _checked_bits(checked_bits),
        _invert_mask(invert_mask),
        _take_if_true(take_if_true),
        _counter(counter) {}
};

struct InputParam {
  uint32_t _pnum;
  IrType _type;
};

struct IrBlock {
  std::vector<IrInst> _insts;
  std::vector<BlockTransition> _tr_out;
};

struct IrGraph {
  std::vector<IrBlock> _blocks;
  GPRBindTracker _gpr_binds;
  FPRBindTracker _fpr_binds;
  std::vector<InputParam> _params;

  IrGraph(ppc::Subroutine const& routine) : _gpr_binds(routine), _fpr_binds(routine) {
    _blocks.resize(routine._graph->_nodes_by_id.size());
  }
};

IrGraph translate_subroutine(ppc::Subroutine const& routine);
}  // namespace decomp::ir
