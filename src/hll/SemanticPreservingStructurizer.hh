#pragma once

#include <unordered_map>
#include <vector>

#include "hll/Structurizer.hh"

namespace decomp::hll {
// Semantics-preserving structural analysis described in the paper
// "Native x86 Decompilation Using Semantics-Preserving Structural Analysis and Iterative Control-Flow Structuring"
// Contains some additional refinement steps
class SemanticPreservingStructurizer : public ControlFlowStructurizer {
public:
  HLLControlTree structurize() override;

private:

  ACNGraph _gr;
  int _post_ctr;
  std::unordered_map<AbstractControlNode*, AbstractControlNode*> _structof;
  std::vector<int> _post;
  std::vector<int> _dom;
  std::vector<int> _pdom;
};
}  // namespace decomp::hll
