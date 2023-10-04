#include "RegisterBinding.hh"

#include <algorithm>
#include <iterator>
#include <queue>
#include <set>
#include <variant>

#include "BinaryContext.hh"
#include "DataSource.hh"
#include "FlagsEnum.hh"
#include "PpcDisasm.hh"
#include "SubroutineGraph.hh"
#include "producers/RandomAccessData.hh"

namespace decomp {
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
constexpr GprSet kKilledByCall = kCallerSavedGpr - kReturnSet;

namespace {
void eval_bindings_local(BasicBlock* block, BinaryContext const& ctx) {
  RegisterLifetimes* rlt;
  if (block->extension_data == nullptr) {
    rlt = new RegisterLifetimes();
    block->extension_data = rlt;
  }

  GprSet inputs;
  GprSet outputs;
  GprSet def_mask;

  for (size_t i = 0; i < block->instructions.size(); i++) {
    MetaInst const& inst = block->instructions[i];

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
      for (DataSource const& read : inst._reads) {
        if (std::holds_alternative<GPRSlice>(read)) {
          use += std::get<GPRSlice>(read)._reg;
        } else if (std::holds_alternative<MemRegReg>(read)) {
          use += gpr_mask(std::get<MemRegReg>(read)._base, std::get<MemRegReg>(read)._offset);
        } else if (std::holds_alternative<MemRegOff>(read)) {
          use += std::get<MemRegOff>(read)._base;
        }
      }
      for (DataSource const& write : inst._writes) {
        if (std::holds_alternative<GPRSlice>(write)) {
          def += std::get<GPRSlice>(write)._reg;
        } else if (std::holds_alternative<MemRegReg>(write)) {
          use += gpr_mask(std::get<MemRegReg>(write)._base, std::get<MemRegReg>(write)._offset);
        } else if (std::holds_alternative<MemRegOff>(write)) {
          use += std::get<MemRegOff>(write)._base;
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
}

bool backpropagate_outputs(BasicBlock* block) {
  RegisterLifetimes* rlt = static_cast<RegisterLifetimes*>(block->extension_data);

  GprSet outedge_inputs;
  for (auto&& [_, outgoing] : block->outgoing_edges) {
    outedge_inputs += static_cast<RegisterLifetimes*>(outgoing->extension_data)->_input;
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
    for (size_t i = 0; i < block->instructions.size(); i++) {
      rlt->_live_in[i] += used_pt;
      rlt->_live_out[i] += used_pt;
    }
    return true;
  }

  return false;
}

bool propagate_guesses(BasicBlock* block) {
  RegisterLifetimes* rlt = static_cast<RegisterLifetimes*>(block->extension_data);

  GprSet passthrough_inputs;
  for (auto&& [_, incoming] : block->incoming_edges) {
    passthrough_inputs += static_cast<RegisterLifetimes*>(incoming->extension_data)->_guess_out +
                          static_cast<RegisterLifetimes*>(incoming->extension_data)->_propagated;
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

void clear_unused_bindings(BasicBlock* block) {
  RegisterLifetimes* rlt = static_cast<RegisterLifetimes*>(block->extension_data);

  // Sweep through the block to clear out liveness for unused sections, E.G.
  // D=Def U=Use .=Neither
  // .....D.............U.........U............D......U.....
  //                              |____________|
  //                              unused section
  GprSet unused_mask = rlt->_guess_out;
  for (size_t i = block->instructions.size(); i > 0; i--) {
    rlt->_live_out[i - 1] -= unused_mask;
    unused_mask = unused_mask + rlt->_def[i - 1] - rlt->_use[i - 1];
    rlt->_live_in[i - 1] -= unused_mask;
  }
}
}  // namespace

void evaluate_bindings(SubroutineGraph& graph, BinaryContext const& ctx) {
  dfs_forward([&ctx](BasicBlock* cur) { eval_bindings_local(cur, ctx); }, always_iterate, graph.root);

  bool did_change;
  do {
    did_change = false;
    dfs_forward([&did_change](BasicBlock* cur) { did_change |= propagate_guesses(cur); }, always_iterate, graph.root);
  } while (did_change);

  do {
    did_change = false;
    dfs_forward(
        [&did_change](BasicBlock* cur) { did_change |= backpropagate_outputs(cur); }, always_iterate, graph.root);
  } while (did_change);

  dfs_forward([](BasicBlock* cur) { clear_unused_bindings(cur); }, always_iterate, graph.root);
}
}  // namespace decomp
