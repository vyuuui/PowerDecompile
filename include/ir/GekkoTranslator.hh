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
  uint32_t _src_idx;
  uint32_t _target_idx;
  std::optional<ConditionTVRef> _cond;
  bool _inv_cond;
  bool _take_if_true;
  CounterCheck _counter;

  BlockTransition(uint32_t src_idx,
    uint32_t target_idx,
    ConditionTVRef cond,
    bool inv_cond,
    bool take_if_true,
    CounterCheck counter = CounterCheck::kIgnore)
      : _src_idx(src_idx),
        _target_idx(target_idx),
        _cond(cond),
        _inv_cond(inv_cond),
        _take_if_true(take_if_true),
        _counter(counter) {}

  BlockTransition(
    uint32_t src_idx, uint32_t target_idx, bool take_if_true, CounterCheck counter = CounterCheck::kIgnore)
      : _src_idx(src_idx), _target_idx(target_idx), _inv_cond(false), _take_if_true(take_if_true), _counter(counter) {}
};

struct IrBlock {
  uint32_t _id;
  std::vector<IrInst> _insts;
  std::vector<BlockTransition> _tr_out;
  std::vector<int> _tr_in;
};

struct IrRoutine {
  std::vector<IrBlock> _blocks;
  uint32_t _root_id;
  GPRBindTracker _gpr_binds;
  FPRBindTracker _fpr_binds;
  CRBindTracker _cr_binds;
  std::array<uint32_t, 8> _int_param;
  std::array<uint32_t, 13> _flt_param;
  std::vector<uint16_t> _stk_params;
  uint8_t _num_int_param;
  uint8_t _num_flt_param;

  IrRoutine(ppc::Subroutine const& routine)
      : _gpr_binds(routine), _fpr_binds(routine), _cr_binds(routine), _num_int_param(0), _num_flt_param(0) {
    _blocks.resize(routine._graph->_nodes_by_id.size());
    for (size_t i = 0; i < _blocks.size(); i++) {
      _blocks[i]._id = i;
    }
    std::fill(_int_param.begin(), _int_param.end(), kInvalidTmp);
    std::fill(_flt_param.begin(), _flt_param.end(), kInvalidTmp);
  }

  template <typename SetType>
  constexpr BindTracker<typename SetType::RegType>& get_binds() {
    if constexpr (std::is_same_v<SetType, ppc::GprSet>) {
      return _gpr_binds;
    } else if constexpr (std::is_same_v<SetType, ppc::FprSet>) {
      return _fpr_binds;
    } else if constexpr (std::is_same_v<SetType, ppc::CrSet>) {
      return _cr_binds;
    }
  }

  constexpr uint32_t param_idx(GPRBindInfo const* gprb) const { return static_cast<uint8_t>(gprb->_reg) - 3; }
  constexpr uint32_t param_idx(FPRBindInfo const* fprb) const {
    return static_cast<uint8_t>(fprb->_reg) - 1 + _num_int_param;
  }
  uint32_t param_idx(uint16_t stack_off) const {
    return std::distance(_stk_params.begin(), std::find(_stk_params.begin(), _stk_params.end(), stack_off)) +
           _num_int_param + _num_flt_param;
  }
};

IrRoutine translate_subroutine(ppc::Subroutine const& routine);
}  // namespace decomp::ir
