#pragma once

#include "hll/Structurizer.hh"

// Non-structurizer, just emits GOTOs between all blocks, outputting them in some topological order
namespace decomp::hll {
class GotoStructurizer : public ControlFlowStructurizer {
public:
  HLLControlTree structurize() override;
};
}  // namespace decomp::hll
