#pragma once

#include <cstdint>
#include <vector>

#include "ir/IrInst.hh"
#include "ir/RegisterBinding.hh"
#include "ppc/Subroutine.hh"
#include "ppc/SubroutineGraph.hh"
#include "utl/FlowGraph.hh"
#include "utl/VariantOverloaded.hh"

namespace decomp::ir {
enum class CounterCheck {
  kCounterIgnore,
  kCounterZero,
  kCounterNotZero,
};

struct IrBlock {
  std::vector<IrInst> _insts;
  // Condition check info
  std::optional<ConditionTVRef> _cond;
  bool _inv_cond;
  CounterCheck _ctr;
};
using IrBlockVertex = FlowVertex<IrBlock>;

struct IrRoutine {
  FlowGraph<IrBlock> _graph;
  GPRBindTracker _gpr_binds;
  FPRBindTracker _fpr_binds;
  CRBindTracker _cr_binds;
  std::array<uint32_t, 8> _int_param;
  std::array<uint32_t, 13> _flt_param;
  std::vector<uint16_t> _stk_params;
  uint8_t _num_int_param;
  uint8_t _num_flt_param;

  IrRoutine(ppc::Subroutine const& routine)
      : _gpr_binds(*routine._graph),
        _fpr_binds(*routine._graph),
        _cr_binds(*routine._graph),
        _num_int_param(0),
        _num_flt_param(0) {
    for (size_t i = 0; i < routine._graph->size(); i++) {
      _graph.emplace_vertex();
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
