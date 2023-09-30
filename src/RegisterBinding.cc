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
// process each block independent of other blocks, collect the following:
//   -> Uses without def = Input temp list
//   -> Defines within block
//   -> Defines not overwritten before block end = Output temp list
// Propagation outwards
//   -> If an output var from an input node is in the input var list, merge the two temps to a single grouped var
//   -> ElseIf an output var from an input **node** is not clobbered by this node, advertise it in the output temp list
//   -> Else no propagation
//   -> Continue while propagation still occurs
// Resolving return values
//   -> Mark r3/r4 usages before block end that are untouched
//   -> propagate "possible usages" alongside real usages
//   -> if a possible return val is used in an output block, transform it from retval to passthrough

constexpr GprSet kReturnSet = gpr_mask_literal<GPR::kR3, GPR::kR4>();
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
// constexpr FprSet kCallerSavedFpr = fpr_mask_literal<FPR::kF0,
//     FPR::kF1,
//     FPR::kF2,
//     FPR::kF3,
//     FPR::kF4,
//     FPR::kF5,
//     FPR::kF6,
//     FPR::kF7,
//     FPR::kF8,
//     FPR::kF9,
//     FPR::kF10,
//     FPR::kF11,
//     FPR::kF12,
//     FPR::kF13>();

namespace {
void process_block(BasicBlock* block, BinaryContext const& ctx) {
  RegisterLifetimes* bindings;
  if (block->extension_data == nullptr) {
    bindings = new RegisterLifetimes();
    block->extension_data = bindings;
  }

  GprSet block_inputs;
  GprSet block_outputs;
  GprSet defined_mask;
  std::optional<size_t> last_call_index;

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
    if (!bindings->_live_out.empty()) {
      live_in = bindings->_live_out.back();
    }

    // TODO: floating point, control fields
    // Function calls => kill caller saves
    if (check_flags(inst._flags, InstFlags::kWritesLR)) {
      if (inst._op == InstOperation::kB && !is_abi_routine(ctx, inst.branch_target())) {
        kill += kCallerSavedGpr;
        last_call_index = i;
      } else if (inst._op == InstOperation::kBclr || inst._op == InstOperation::kBc ||
                 inst._op == InstOperation::kBcctr) {
        kill += kCallerSavedGpr;
        last_call_index = i;
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

    // Register is a return value when
    //  1. The register referenced is a return register
    //  2. The register referenced is not live coming into this instruction
    //  3. There was a call prior to this instruction
    GprSet used_rets = use & kReturnSet;
    if (last_call_index && !used_rets.empty() && (live_in & used_rets).empty()) {
      bindings->_def[*last_call_index] += used_rets;
      bindings->_kill[*last_call_index] -= used_rets;
      for (size_t j = *last_call_index; j < i; j++) {
        bindings->_live_out[j] += used_rets;
        bindings->_live_in[j + 1] = bindings->_live_out[j];
      }
    }

    defined_mask += kill + def;
    block_inputs += use - defined_mask;
    block_outputs = (block_outputs - kill + def + use);

    // Return values must be the result of a kill, excluding things defined and used
    bindings->_untouched_retval += (kill - def - use) & kReturnSet;
    bindings->_def.push_back(def);
    bindings->_use.push_back(use);
    bindings->_kill.push_back(kill);

    bindings->_live_in.push_back(live_in);
    bindings->_live_out.push_back(live_in + use + def - kill);
  }

  bindings->_input = block_inputs;
  bindings->_output = block_outputs;
  bindings->_overwritten = defined_mask;
}

bool backpropagate_retvals(BasicBlock* block) {
  RegisterLifetimes* lifetimes = static_cast<RegisterLifetimes*>(block->extension_data);

  GprSet retval_outputs;
  for (auto&& [_, outgoing] : block->outgoing_edges) {
    retval_outputs += static_cast<RegisterLifetimes*>(outgoing->extension_data)->_input;
  }
  retval_outputs &= kReturnSet;

  // Any outputs that overlap with this block's untouched retvals should backpropagate
  GprSet confirmed_ret_usages = retval_outputs & lifetimes->_untouched_retval;

  // There is nothing to backpropagate
  if (confirmed_ret_usages.empty()) {
    return false;
  }

  for (size_t i = 0; i < lifetimes->_live_in.size(); i++) {
    lifetimes->_live_in[i] += confirmed_ret_usages;
    lifetimes->_live_out[i] += confirmed_ret_usages;
  }

  lifetimes->_input += confirmed_ret_usages;
  lifetimes->_output += confirmed_ret_usages;
  lifetimes->_untouched_retval -= confirmed_ret_usages;

  return true;
}

bool propagate_block(BasicBlock* block) {
  RegisterLifetimes* lifetimes = static_cast<RegisterLifetimes*>(block->extension_data);

  GprSet passthrough_inputs;
  GprSet passthrough_retvals;
  for (auto&& [_, incoming] : block->incoming_edges) {
    passthrough_inputs += static_cast<RegisterLifetimes*>(incoming->extension_data)->_output;
    passthrough_retvals += static_cast<RegisterLifetimes*>(incoming->extension_data)->_untouched_retval;
  }

  // Kill any registers that are overwritten in this block, excluding any inputs
  lifetimes->_killed_at_entry += (passthrough_inputs - lifetimes->_input) & lifetimes->_overwritten;
  // Additional inputs should only include new (passthrough) registers
  passthrough_inputs -= lifetimes->_killed_at_entry + lifetimes->_input;
  passthrough_retvals -= lifetimes->_input + lifetimes->_overwritten;

  if (passthrough_inputs.empty() && lifetimes->_untouched_retval == passthrough_retvals) {
    return backpropagate_retvals(block);
  }

  lifetimes->_untouched_retval += passthrough_retvals;

  if (!passthrough_inputs.empty()) {
    for (size_t i = 0; i < lifetimes->_live_in.size(); i++) {
      lifetimes->_live_in[i] += passthrough_inputs;
      lifetimes->_live_out[i] += passthrough_inputs;
    }

    lifetimes->_input += passthrough_inputs;
    lifetimes->_output += passthrough_inputs;
  }
  return true;
}

}  // namespace

void evaluate_bindings(SubroutineGraph& graph, BinaryContext const& ctx) {
  dfs_forward(
      [&ctx](BasicBlock* cur) { process_block(cur, ctx); }, [](BasicBlock*, BasicBlock*) { return true; }, graph.root);

  bool did_change;
  do {
    did_change = false;
    dfs_forward([&did_change](BasicBlock* cur) { did_change |= propagate_block(cur); },
        [](BasicBlock*, BasicBlock*) { return true; },
        graph.root);
  } while (did_change);
}
}  // namespace decomp
