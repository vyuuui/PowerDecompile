#pragma once

#include <cstdint>

#include "RegSet.hh"
#include "StackAnalysis.hh"
#include "SubroutineGraph.hh"

namespace decomp {
// Encapsulation of all data pertaining to a subroutine
struct Subroutine {
  uint32_t _start_va;
  SubroutineGraph _graph;

  SubroutineStack _stack;
  GprSet _gpr_param;
  FprSet _fpr_param;
  // TODO: moveme
  std::vector<StackVariable> _stack_params;
};
}  // namespace decomp
