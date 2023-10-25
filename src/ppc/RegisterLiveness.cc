#include "ppc/RegisterLiveness.hh"

#include <algorithm>
#include <iterator>
#include <queue>
#include <set>
#include <variant>

#include "ppc/BinaryContext.hh"
#include "ppc/DataSource.hh"
#include "ppc/PpcDisasm.hh"
#include "ppc/SubroutineGraph.hh"
#include "producers/RandomAccessData.hh"
#include "utl/FlagsEnum.hh"

namespace decomp::ppc {
namespace {
void eval_liveness_local(BasicBlock* block, BinaryContext const& ctx) {
  RegisterLiveness* rlt;
  if (block->_block_lifetimes == nullptr) {
    rlt = new RegisterLiveness();
    block->_block_lifetimes = rlt;
  }
  rlt = block->_block_lifetimes;

  GprSet inputs;
  GprSet outputs;
  GprSet def_mask;

  for (size_t i = 0; i < block->_instructions.size(); i++) {
    MetaInst const& inst = block->_instructions[i];

    // Naming convention:
    // live_in: Registers live coming into this instruction
    // use: Register set accessed by this instruction
    // def: Register set modified by this instruction
    // kill: Register set killed with an unknown value by this instruction
    GprSet live_in;
    GprSet use;
    GprSet def;
    GprSet kill;

    // Pre-fill live_in with previous instruction's live_out set
    if (!rlt->_live_out.empty()) {
      live_in = rlt->_live_out.back();
    }

    // TODO: floating point, control fields
    // Function calls => kill caller saves
    if (check_flags(inst._flags, InstFlags::kWritesLR)) {
      if (inst._op == InstOperation::kB && !is_abi_routine(ctx, inst.branch_target())) {
        kill = kKilledByCall;
        def = kReturnSet;
      } else if (inst._op == InstOperation::kBclr || inst._op == InstOperation::kBc ||
                 inst._op == InstOperation::kBcctr) {
        kill = kKilledByCall;
        def = kReturnSet;
      }
    } else {
      for (ReadSource const& read : inst._reads) {
        if (std::holds_alternative<GPRSlice>(read)) {
          use += std::get<GPRSlice>(read)._reg;
        } else if (std::holds_alternative<MemRegReg>(read)) {
          use += gpr_mask(std::get<MemRegReg>(read)._base, std::get<MemRegReg>(read)._offset);
        } else if (std::holds_alternative<MemRegOff>(read)) {
          use += std::get<MemRegOff>(read)._base;
        } else if (std::holds_alternative<MultiReg>(read)) {
          use += gpr_range(std::get<MultiReg>(read)._low);
        }
      }
      for (WriteSource const& write : inst._writes) {
        if (std::holds_alternative<GPRSlice>(write)) {
          def += std::get<GPRSlice>(write)._reg;
        } else if (std::holds_alternative<MemRegReg>(write)) {
          use += gpr_mask(std::get<MemRegReg>(write)._base, std::get<MemRegReg>(write)._offset);
        } else if (std::holds_alternative<MemRegOff>(write)) {
          use += std::get<MemRegOff>(write)._base;
        } else if (std::holds_alternative<MultiReg>(write)) {
          def += gpr_range(std::get<MultiReg>(write)._low);
        }
      }
      // Any updating write does not count as a define
      def -= use;
    }

    def_mask += kill + def;
    inputs += use - def_mask;
    outputs = (outputs - kill + def + use);

    rlt->_def.push_back(def);
    rlt->_use.push_back(use);

    rlt->_live_in.push_back(live_in);
    rlt->_live_out.push_back(live_in + use + def - kill);
  }

  rlt->_input = inputs;
  rlt->_guess_out = outputs;
  rlt->_overwrite = def_mask;

  GprSet input_mask = inputs;
  for (size_t i = 0; input_mask && i < block->_instructions.size(); i++) {
    rlt->_live_in[i] += input_mask;
    input_mask -= rlt->_use[i];
    rlt->_live_out[i] += input_mask;
  }
}

bool backpropagate_outputs(BasicBlock* block) {
  RegisterLiveness* rlt = block->_block_lifetimes;

  GprSet outedge_inputs;
  for (auto&& [_, outgoing] : block->_outgoing_edges) {
    outedge_inputs += outgoing->_block_lifetimes->_input;
  }

  GprSet used_out = outedge_inputs & rlt->_guess_out;
  if (used_out) {
    rlt->_guess_out -= used_out;
    rlt->_output += used_out;
  }

  GprSet used_pt = outedge_inputs & rlt->_propagated;
  if (used_pt) {
    rlt->_propagated -= used_pt;
    rlt->_output += used_pt;
    rlt->_input += used_pt;
    for (size_t i = 0; i < block->_instructions.size(); i++) {
      rlt->_live_in[i] += used_pt;
      rlt->_live_out[i] += used_pt;
    }
    return true;
  }

  return false;
}

bool propagate_guesses(BasicBlock* block) {
  RegisterLiveness* rlt = block->_block_lifetimes;

  GprSet passthrough_inputs;
  for (auto&& [_, incoming] : block->_incoming_edges) {
    passthrough_inputs += incoming->_block_lifetimes->_guess_out + incoming->_block_lifetimes->_propagated;
  }

  // Additional inputs should only include new (passthrough) registers
  passthrough_inputs -= rlt->_overwrite + rlt->_input;

  // There was nothing new to propagate here
  if (passthrough_inputs == rlt->_propagated) {
    return false;
  }

  rlt->_propagated = passthrough_inputs;
  return true;
}

void clear_unused_sections(BasicBlock* block) {
  RegisterLiveness* rlt = block->_block_lifetimes;

  // Sweep through the block to clear out liveness for unused sections, E.G.
  // D=Def U=Use .=Neither
  // .....D.............U.........U............D......U.....
  //                              |____________|
  //                              unused section
  GprSet unused_mask = rlt->_guess_out;
  for (size_t i = block->_instructions.size(); i > 0; i--) {
    rlt->_live_out[i - 1] -= unused_mask;
    unused_mask = unused_mask + rlt->_def[i - 1] - rlt->_use[i - 1];
    rlt->_live_in[i - 1] -= unused_mask;
  }
}
}  // namespace

void run_liveness_analysis(SubroutineGraph& graph, BinaryContext const& ctx) {
  dfs_forward([&ctx](BasicBlock* cur) { eval_liveness_local(cur, ctx); }, always_iterate, graph._root);

  bool did_change;
  do {
    did_change = false;
    dfs_forward([&did_change](BasicBlock* cur) { did_change |= propagate_guesses(cur); }, always_iterate, graph._root);
  } while (did_change);

  do {
    did_change = false;
    dfs_forward(
      [&did_change](BasicBlock* cur) { did_change |= backpropagate_outputs(cur); }, always_iterate, graph._root);
  } while (did_change);

  dfs_forward([](BasicBlock* cur) { clear_unused_sections(cur); }, always_iterate, graph._root);
}
}  // namespace decomp::ppc
