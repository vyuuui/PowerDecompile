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
constexpr GprSet kAbiRegs = ppc::gpr_mask_literal<ppc::GPR::kR1, ppc::GPR::kR2, ppc::GPR::kR13>();
constexpr GprSet kReturnSet = gpr_mask_literal<GPR::kR3, GPR::kR4>();
constexpr GprSet kParameterSet =
  gpr_mask_literal<GPR::kR3, GPR::kR4, GPR::kR5, GPR::kR6, GPR::kR7, GPR::kR8, GPR::kR9, GPR::kR10>();
constexpr GprSet kCallerSavedGpr = gpr_mask_literal<GPR::kR0,
  GPR::kR3,
  GPR::kR4,
  GPR::kR5,
  GPR::kR6,
  GPR::kR7,
  GPR::kR8,
  GPR::kR9,
  GPR::kR10,
  GPR::kR11,
  GPR::kR12>();
constexpr GprSet kCalleeSavedGpr = kAllGprs - kCallerSavedGpr - kAbiRegs;
constexpr GprSet kKilledByCall = kCallerSavedGpr - kReturnSet;

struct RegisterLiveness {
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

void run_liveness_analysis(SubroutineGraph& graph, BinaryContext const& ctx);
}  // namespace decomp::ppc
