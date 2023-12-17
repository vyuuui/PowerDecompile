#include "hll/CFAnalyzer.hh"

#include <vector>

namespace decomp::hll {
namespace {
struct Loop {
  // loop head
  ir::IrBlock* _header;
  // Backedges of this loop for this loop
  std::vector<ir::BlockTransition*> _backedge;
  // Edges which exit the loop
  std::vector<ir::BlockTransition*> _exits;
  // LCA of all exit points, to determine the post-loop section
  ir::IrBlock* _exit_lca;
};

// Preprocess an IR routine for all loops
std::vector<Loop> preprocess_routine(ir::IrRoutine const& routine) {
  std::vector<Loop> ret;

  std::vector<ir::IrBlock const*> proc;
  proc.push_back(&routine._blocks[routine._root_id]);

  while (!proc.empty()) {
    ir::IrBlock const* block = proc.back();
    proc.pop_back();


  }
}
}  // namespace

void run_control_flow_analysis(ir::IrRoutine const& routine) {
}
}  // namespace decomp::hll
