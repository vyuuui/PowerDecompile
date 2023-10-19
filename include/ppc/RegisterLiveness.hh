#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "ppc/BinaryContext.hh"
#include "ppc/DataSource.hh"
#include "ppc/RegSet.hh"
#include "ppc/SubroutineGraph.hh"
#include "producers/RandomAccessData.hh"

namespace decomp::ppc {
struct RegisterLifetimes {
  // Per-instruction register liveness
  std::vector<GprSet> _def;
  std::vector<GprSet> _use;
  std::vector<GprSet> _live_in;
  std::vector<GprSet> _live_out;

  // Liveness summary for whole block
  GprSet _input;
  GprSet _output;
  GprSet _overwrite;

  // Temporary usage fields
  GprSet _guess_out;
  GprSet _propagated;
};

void evaluate_bindings(SubroutineGraph& graph, BinaryContext const& ctx);
}  // namespace decomp::ppc
