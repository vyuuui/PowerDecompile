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
// REWORK:
// process each block independent of other blocks, collect the following:
//   -> Uses without def = Input temp list
//   -> Defines within block
//   -> Defines not overwritten before block end = Output temp list
// Propagation outwards
//   -> If an output var from an input node is in the input var list, merge the two temps to a single grouped var
//   -> ElseIf an output var from an input **node** is not clobbered by this node, advertise it in the output temp list
//   -> Else no propagation
//   -> Continue while propagation still occurs

template <GPR... gprs>
constexpr RegSet<GPR> gpr_mask_literal() {
  return RegSet<GPR>{(0 | ... | (1 << static_cast<uint8_t>(gprs)))};
}
template <FPR... gprs>
constexpr RegSet<FPR> fpr_mask_literal() {
  return RegSet<FPR>{(0 | ... | (1 << static_cast<uint8_t>(gprs)))};
}
template <typename... Ts>
constexpr RegSet<GPR> gpr_mask(Ts... args) {
  return RegSet<GPR>{(0 | ... | static_cast<uint32_t>(1 << static_cast<uint8_t>(args)))};
}
template <typename... Ts>
constexpr RegSet<FPR> fpr_mask(Ts... args) {
  return RegSet<FPR>{(0 | ... | static_cast<uint32_t>(1 << static_cast<uint8_t>(args)))};
}

constexpr RegSet<GPR> kCallerSavedGpr = gpr_mask_literal<GPR::kR0,
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
// constexpr RegSet<FPR> kCallerSavedFpr = fpr_mask_literal<FPR::kF0,
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
uint32_t branch_target(MetaInst const& inst, uint32_t addr) {
  return addr + std::get<RelBranch>(inst._immediates[0])._rel_32;
}

void process_block(BasicBlock* block, BinaryContext const& ctx) {
  RegisterLifetimes* bindings;
  if (block->extension_data == nullptr) {
    bindings = new RegisterLifetimes();
    block->extension_data = bindings;
  }

  RegSet<GPR> block_inputs;
  RegSet<GPR> block_outputs;
  RegSet<GPR> defined_mask;

  for (size_t i = 0; i < block->instructions.size(); i++) {
    uint32_t addr = block->block_start + static_cast<uint32_t>(i * 4);
    MetaInst const& inst = block->instructions[i];

    // Naming convention:
    // live_in: Registers live coming into this instruction
    // use: Register set accessed by this instruction
    // def: Register set modified by this instruction
    // kill: Register set killed with an unknown value by this instruction
    RegSet<GPR> live_in;
    RegSet<GPR> use;
    RegSet<GPR> def;
    RegSet<GPR> kill;

    // Pre-fill live_in with previous instruction's live_out set
    if (!bindings->_live_out.empty()) {
      live_in = bindings->_live_out.back();
    }

    // Function calls => kill caller saves
    // TODO: guess plausible returns
    // TODO: floating point, control fields
    if (check_flags(inst._flags, InstFlags::kWritesLR)) {
      if (inst._op == InstOperation::kB && !is_abi_routine(ctx, branch_target(inst, addr))) {
        kill += kCallerSavedGpr;
      } else if (inst._op == InstOperation::kBclr || inst._op == InstOperation::kBc ||
                 inst._op == InstOperation::kBcctr) {
        kill += kCallerSavedGpr;
      }
    } else {
      for (DataSource const& read : inst._reads) {
        if (std::holds_alternative<GPR>(read)) {
          use += std::get<GPR>(read);
        } else if (std::holds_alternative<MemRegReg>(read)) {
          use += gpr_mask(std::get<MemRegReg>(read)._base, std::get<MemRegReg>(read)._offset);
        } else if (std::holds_alternative<MemRegOff>(read)) {
          use += std::get<MemRegOff>(read)._base;
        }
      }
      for (DataSource const& write : inst._writes) {
        if (std::holds_alternative<GPR>(write)) {
          def += std::get<GPR>(write);
        } else if (std::holds_alternative<MemRegReg>(write)) {
          use += gpr_mask(std::get<MemRegReg>(write)._base, std::get<MemRegReg>(write)._offset);
        } else if (std::holds_alternative<MemRegOff>(write)) {
          use += std::get<MemRegOff>(write)._base;
        }
      }
      // Any updating write does not count as a define
      def -= use;
    }

    defined_mask += kill + def;
    block_inputs += use - defined_mask;
    block_outputs = (block_outputs - kill + def + use);

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

bool propagate_block(BasicBlock* block) {
  RegisterLifetimes* lifetimes = static_cast<RegisterLifetimes*>(block->extension_data);

  RegSet<GPR> additional_inputs;
  for (auto&& [_, incoming] : block->incoming_edges) {
    additional_inputs += static_cast<RegisterLifetimes*>(incoming->extension_data)->_output;
  }
  // Kill any registers that are overwritten in this block, excluding any inputs
  lifetimes->_killed_at_entry += (additional_inputs - lifetimes->_input) & lifetimes->_overwritten;
  // Additional inputs should only include new (passthrough) registers
  additional_inputs -= lifetimes->_killed_at_entry + lifetimes->_input;

  if (additional_inputs.empty()) {
    return false;
  }

  for (size_t i = 0; i < lifetimes->_live_in.size(); i++) {
    lifetimes->_live_in[i] += additional_inputs;
    lifetimes->_live_out[i] += additional_inputs;
  }

  lifetimes->_input += additional_inputs;
  lifetimes->_output += additional_inputs;
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
