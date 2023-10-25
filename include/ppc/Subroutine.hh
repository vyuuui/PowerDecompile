#pragma once

#include <cstdint>

#include "ppc/RegSet.hh"
#include "ppc/SubroutineGraph.hh"
#include "ppc/SubroutineStack.hh"

namespace decomp::ppc {
// Encapsulation of all data pertaining to a subroutine
struct Subroutine {
  uint32_t _start_va;
  SubroutineGraph _graph;

  SubroutineStack _stack;
  GprSet _gpr_param;
  FprSet _fpr_param;
};
}  // namespace decomp::ppc
