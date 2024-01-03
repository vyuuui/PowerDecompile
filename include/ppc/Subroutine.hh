#pragma once

#include <cstdint>
#include <memory>

#include "ppc/RegSet.hh"

namespace decomp::ppc {
class SubroutineGraph;
class SubroutineStack;

// Encapsulation of all data pertaining to a subroutine
struct Subroutine {
  uint32_t _start_va;
  std::unique_ptr<SubroutineGraph> _graph;
  std::unique_ptr<SubroutineStack> _stack;
  GprSet _gpr_param;
  FprSet _fpr_param;
};
}  // namespace decomp::ppc
