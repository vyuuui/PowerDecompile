#include "hll/Structurizer.hh"

#include <limits>
#include <numeric>
#include <set>
#include <vector>

namespace decomp::hll {
void ControlFlowStructurizer::prepare(ir::IrRoutine const& routine) {
  _routine = &routine;
  for (ir::IrBlockVertex const& block : routine._graph) {
    _leaves.push_back(new BasicBlock(block));
  }
}

HLLControlTree run_control_flow_analysis(ControlFlowStructurizer* structurizer, ir::IrRoutine const& routine) {
  structurizer->prepare(routine);
  return structurizer->structurize();
}
}  // namespace decomp::hll
