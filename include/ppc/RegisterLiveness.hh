#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "ppc/BinaryContext.hh"
#include "ppc/DataSource.hh"
#include "ppc/RegSet.hh"
#include "ppc/Subroutine.hh"
#include "producers/RandomAccessData.hh"

namespace decomp::ppc {
// GPR sets
constexpr GprSet kAbiRegs = ppc::gpr_mask_literal<ppc::GPR::kR1, ppc::GPR::kR2, ppc::GPR::kR13>();
constexpr GprSet kReturnSetGpr = gpr_mask_literal<GPR::kR3, GPR::kR4>();
constexpr GprSet kParameterSetGpr =
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
constexpr GprSet kKilledByCallGpr = kCallerSavedGpr - kReturnSetGpr;

// FPR sets
constexpr FprSet kReturnSetFpr = fpr_mask_literal<FPR::kF1, FPR::kF2, FPR::kF3, FPR::kF4>();
constexpr FprSet kParameterSetFpr = fpr_mask_literal<FPR::kF1,
  FPR::kF2,
  FPR::kF3,
  FPR::kF4,
  FPR::kF5,
  FPR::kF6,
  FPR::kF7,
  FPR::kF8,
  FPR::kF9,
  FPR::kF10,
  FPR::kF11,
  FPR::kF12,
  FPR::kF13>();
constexpr FprSet kCallerSavedFpr = kParameterSetFpr + FPR::kF1;
constexpr FprSet kCalleeSavedFpr = kAllFprs - kCallerSavedFpr;
constexpr FprSet kKilledByCallFpr = kCallerSavedFpr - kReturnSetFpr;

// Generic liveness tracker
template <typename RegType>
struct RegisterLiveness {
  // Per-instruction register liveness
  std::vector<RegSet<RegType>> _def;
  std::vector<RegSet<RegType>> _use;
  std::vector<RegSet<RegType>> _live_in;
  std::vector<RegSet<RegType>> _live_out;

  // Liveness summary for whole block
  RegSet<RegType> _input;
  RegSet<RegType> _output;
  RegSet<RegType> _overwrite;
  RegSet<RegType> _routine_inputs;

  // Temporary usage fields
  RegSet<RegType> _guess_out;
  RegSet<RegType> _propagated;
};

using GprLiveness = RegisterLiveness<GPR>;
using FprLiveness = RegisterLiveness<FPR>;

void run_liveness_analysis(Subroutine& routine, BinaryContext const& ctx);
}  // namespace decomp::ppc
