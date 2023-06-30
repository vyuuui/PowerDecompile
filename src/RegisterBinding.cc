#include "RegisterBinding.hh"

#include <algorithm>
#include <iterator>
#include <queue>
#include <set>
#include <variant>

#include "CodeWarriorABIConfiguration.hh"
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
constexpr RegSet<GPR> gpr_mask() {
  return RegSet<GPR>{(0 | ... | (1 << static_cast<uint8_t>(gprs)))};
}
template <FPR... gprs>
constexpr RegSet<FPR> fpr_mask() {
  return RegSet<FPR>{(0 | ... | (1 << static_cast<uint8_t>(gprs)))};
}

constexpr RegSet<GPR> kCallerSavedGpr = gpr_mask<GPR::kR0,
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
// constexpr RegSet<FPR> kCallerSavedFpr = fpr_mask<FPR::kF0,
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

bool abi_routine(RandomAccessData const& ram, uint32_t addr) {
  constexpr auto detect_savegpr = [](RandomAccessData const& ram, uint32_t addr) -> bool {
    MetaInst inst = ram.read_instruction(addr);
    if (inst._op != InstOperation::kStw || std::get<GPR>(inst._reads[0]) < GPR::kR14 ||
        std::get<MemRegOff>(inst._writes[0])._base != GPR::kR11) {
      return false;
    }

    int cur_savegpr = static_cast<int>(std::get<GPR>(inst._writes[0])) + 1;
    for (;; addr += 4) {
      inst = ram.read_instruction(addr);
      if (is_blr(inst)) {
        return true;
      }

      if (inst._op != InstOperation::kStw || static_cast<int>(std::get<GPR>(inst._reads[0])) != cur_savegpr ||
          std::get<MemRegOff>(inst._writes[0])._base != GPR::kR11) {
        break;
      }
      cur_savegpr++;
    }
    return false;
  };
  constexpr auto detect_restgpr = [](RandomAccessData const& ram, uint32_t addr) -> bool {
    MetaInst inst = ram.read_instruction(addr);
    if (inst._op != InstOperation::kLwz || std::get<GPR>(inst._writes[0]) < GPR::kR14 ||
        std::get<MemRegOff>(inst._reads[0])._base != GPR::kR11) {
      return false;
    }

    int cur_restgpr = static_cast<int>(std::get<GPR>(inst._reads[0])) + 1;
    for (;; addr += 4) {
      inst = ram.read_instruction(addr);
      if (is_blr(inst)) {
        return true;
      }

      if (inst._op != InstOperation::kStw || static_cast<int>(std::get<GPR>(inst._writes[0])) != cur_restgpr ||
          std::get<MemRegOff>(inst._reads[0])._base != GPR::kR11) {
        break;
      }
      cur_restgpr++;
    }
    return false;
  };
  constexpr auto in_save_rest_range = [](uint32_t save_rest, uint32_t addr) -> bool {
    return (addr - save_rest) >> 2 < 18;
  };

  if ((gAbiConfig._savegpr_start && in_save_rest_range(*gAbiConfig._savegpr_start, addr)) ||
      (gAbiConfig._restgpr_start && in_save_rest_range(*gAbiConfig._restgpr_start, addr))) {
    return true;
  }
  if (detect_savegpr(ram, addr)) {
    return true;
  }
  if (detect_restgpr(ram, addr)) {
    return true;
  }

  return false;
}

void process_block(RandomAccessData const& ram, BasicBlock* block) {
  RegisterLifetimes* bindings;
  if (block->extension_data == nullptr) {
    bindings = new RegisterLifetimes();
    block->extension_data = bindings;
  }

  RegSet<GPR> block_inputs;
  RegSet<GPR> block_outputs;
  RegSet<GPR> defined_mask;

  for (uint32_t addr = block->block_start; addr < block->block_end; addr += 4) {
    MetaInst inst = ram.read_instruction(addr);

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
      if (inst._op == InstOperation::kB && !abi_routine(ram, branch_target(inst, addr))) {
        kill += kCallerSavedGpr;
      } else if (inst._op == InstOperation::kBclr || inst._op == InstOperation::kBc ||
                 inst._op == InstOperation::kBcctr) {
        kill += kCallerSavedGpr;
      }
    } else {
      for (DataSource const& read : inst._reads) {
        if (std::holds_alternative<GPR>(read)) {
          use += std::get<GPR>(read);
        }
      }
      for (DataSource const& write : inst._writes) {
        if (std::holds_alternative<GPR>(write)) {
          def += std::get<GPR>(write);
        }
      }
      // Any updating write does not count as a define
      def -= use;
    }

    defined_mask += kill + def;
    block_inputs += use - defined_mask;
    block_outputs = (block_outputs - kill + def + use);

    bindings->_live_in.push_back(live_in);
    bindings->_live_out.push_back(use + live_in - kill);
    bindings->_kill.push_back(kill);
  }

  bindings->_input = block_inputs;
  bindings->_output = block_outputs;
}

}  // namespace

void evaluate_bindings(RandomAccessData const& ram, SubroutineGraph& graph) {
  dfs_forward(
      [&ram](BasicBlock* cur) { process_block(ram, cur); }, [](BasicBlock*, BasicBlock*) { return true; }, graph.root);
}
}  // namespace decomp
