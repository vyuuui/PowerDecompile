#pragma once

#include <vector>

#include "hll/Structurizer.hh"

// Semantics-preserving structural analysis described in the paper
// "Native x86 Decompilation Using Semantics-Preserving Structural Analysis and Iterative Control-Flow Structuring"
// Contains some additional refinement steps
namespace decomp::hll {
class SemanticPreservingStructurizer : public ControlFlowStructurizer {
public:
  HLLControlTree structurize() override;
};
}  // namespace decomp::hll
