#pragma once

#include <cstdint>
#include <vector>

namespace decomp::ppc {
struct BinaryContext;
struct Subroutine;

enum class PerilogueInstructionType : uint8_t {
  kNormalInst,

  kFrameAllocate,
  kMoveLRToR0,
  kSaveSenderLR,
  kCalleeGPRSave,
  kCalleeFPRSave,
  kCalleeGPRRestore,
  kCalleeFPRRestore,
  kAbiRoutine,
  kLoadSenderLR,
  kMoveR0toLR,
  kFrameDeallocate,
};

void run_perilogue_analysis(Subroutine& subroutine, BinaryContext const& ctx);
}  // namespace decomp::ppc
