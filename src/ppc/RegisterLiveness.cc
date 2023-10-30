#include "ppc/RegisterLiveness.hh"

#include <algorithm>
#include <iterator>
#include <queue>
#include <set>
#include <variant>

#include "ppc/BinaryContext.hh"
#include "ppc/DataSource.hh"
#include "ppc/PpcDisasm.hh"
#include "ppc/Subroutine.hh"
#include "ppc/SubroutineGraph.hh"
#include "producers/RandomAccessData.hh"
#include "utl/FlagsEnum.hh"

namespace decomp::ppc {
namespace {
template <typename SetType>
constexpr RegisterLiveness<typename SetType::RegType>* get_liveness(BasicBlock* block) {
  if constexpr (std::is_same_v<SetType, GprSet>) {
    return block->_gpr_lifetimes.get();
  } else if constexpr (std::is_same_v<SetType, FprSet>) {
    return block->_fpr_lifetimes.get();
  }
}

template <typename SetType>
SetType uses_for_read(ReadSource const& read) {
  SetType uses;
  if constexpr (std::is_same_v<SetType, GprSet>) {
    if (std::holds_alternative<GPRSlice>(read)) {
      uses += std::get<GPRSlice>(read)._reg;
    } else if (std::holds_alternative<MemRegReg>(read)) {
      uses += gpr_mask(std::get<MemRegReg>(read)._base, std::get<MemRegReg>(read)._offset);
    } else if (std::holds_alternative<MemRegOff>(read)) {
      uses += std::get<MemRegOff>(read)._base;
    } else if (std::holds_alternative<MultiReg>(read)) {
      uses += gpr_range(std::get<MultiReg>(read)._low);
    }
  } else if constexpr (std::is_same_v<SetType, FprSet>) {
    if (std::holds_alternative<FPRSlice>(read)) {
      uses += std::get<FPRSlice>(read)._reg;
    }
  }
  return uses;
}

template <typename SetType>
SetType uses_for_write(WriteSource const& write) {
  SetType uses;
  if constexpr (std::is_same_v<SetType, GprSet>) {
    if (std::holds_alternative<MemRegReg>(write)) {
      uses += gpr_mask(std::get<MemRegReg>(write)._base, std::get<MemRegReg>(write)._offset);
    } else if (std::holds_alternative<MemRegOff>(write)) {
      uses += std::get<MemRegOff>(write)._base;
    }
  }
  return uses;
}

template <typename SetType>
SetType defs_for_write(WriteSource const& write) {
  SetType defs;
  if constexpr (std::is_same_v<SetType, GprSet>) {
    if (std::holds_alternative<GPRSlice>(write)) {
      defs += std::get<GPRSlice>(write)._reg;
    } else if (std::holds_alternative<MultiReg>(write)) {
      defs += gpr_range(std::get<MultiReg>(write)._low);
    }
    return defs;
  } else if constexpr (std::is_same_v<SetType, FprSet>) {
    if (std::holds_alternative<FPRSlice>(write)) {
      defs += std::get<FPRSlice>(write)._reg;
    }
  }
  return defs;
}

constexpr struct {
  std::tuple<GprSet, FprSet> _abi_regs = {kAbiRegs, FprSet()};
  std::tuple<GprSet, FprSet> _return_set = {kReturnSetGpr, kReturnSetFpr};
  std::tuple<GprSet, FprSet> _parameter_set = {kParameterSetGpr, kParameterSetFpr};
  std::tuple<GprSet, FprSet> _caller_saved = {kCallerSavedGpr, kCallerSavedFpr};
  std::tuple<GprSet, FprSet> _callee_saved = {kCalleeSavedGpr, kCalleeSavedFpr};
  std::tuple<GprSet, FprSet> _killed_by_caller = {kKilledByCallGpr, kKilledByCallFpr};
} kRegSets;

template <typename SetType>
void eval_liveness_local(BasicBlock* block, BinaryContext const& ctx) {
  auto rlt = get_liveness<SetType>(block);

  SetType inputs;
  SetType outputs;
  SetType def_mask;

  for (size_t i = 0; i < block->_instructions.size(); i++) {
    MetaInst const& inst = block->_instructions[i];

    // Naming convention:
    // live_in: Registers live coming into this instruction
    // use: Register set accessed by this instruction
    // def: Register set modified by this instruction
    // kill: Register set killed with an unknown value by this instruction
    SetType live_in;
    SetType use;
    SetType def;
    SetType kill;

    // Pre-fill live_in with previous instruction's live_out set
    if (!rlt->_live_out.empty()) {
      live_in = rlt->_live_out.back();
    }

    // TODO: floating point, control fields
    // Function calls => kill caller saves
    if (check_flags(inst._flags, InstFlags::kWritesLR)) {
      if (inst._op == InstOperation::kB && !is_abi_routine(ctx, inst.branch_target())) {
        kill = std::get<SetType>(kRegSets._killed_by_caller);
        def = std::get<SetType>(kRegSets._return_set);
      } else if (inst._op == InstOperation::kBclr || inst._op == InstOperation::kBc ||
                 inst._op == InstOperation::kBcctr) {
        kill = std::get<SetType>(kRegSets._killed_by_caller);
        def = std::get<SetType>(kRegSets._return_set);
      }
    } else {
      for (ReadSource const& read : inst._reads) {
        use += uses_for_read<SetType>(read);
      }
      for (WriteSource const& write : inst._writes) {
        use += uses_for_write<SetType>(write);
        def += defs_for_write<SetType>(write);
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

  SetType input_mask = inputs;
  for (size_t i = 0; input_mask && i < block->_instructions.size(); i++) {
    rlt->_live_in[i] += input_mask;
    input_mask -= rlt->_use[i];
    rlt->_live_out[i] += input_mask;
  }
}

template <typename SetType>
bool backpropagate_outputs(BasicBlock* block) {
  auto rlt = get_liveness<SetType>(block);

  SetType outedge_inputs;
  for (auto&& [_, outgoing] : block->_outgoing_edges) {
    outedge_inputs += get_liveness<SetType>(outgoing)->_input;
  }

  SetType used_out = outedge_inputs & rlt->_guess_out;
  if (used_out) {
    rlt->_guess_out -= used_out;
    rlt->_output += used_out;
  }

  SetType used_pt = outedge_inputs & rlt->_propagated;
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

template <typename SetType>
bool propagate_guesses(BasicBlock* block) {
  auto rlt = get_liveness<SetType>(block);

  SetType passthrough_inputs;
  for (auto&& [_, incoming] : block->_incoming_edges) {
    passthrough_inputs += get_liveness<SetType>(incoming)->_guess_out + get_liveness<SetType>(incoming)->_propagated;
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

template <typename SetType>
void clear_unused_sections(BasicBlock* block) {
  auto rlt = get_liveness<SetType>(block);

  // Sweep through the block to clear out liveness for unused sections, E.G.
  // D=Def U=Use .=Neither
  // .....D.............U.........U............D......U.....
  //                              |____________|
  //                              unused section
  SetType unused_mask = rlt->_guess_out;
  for (size_t i = block->_instructions.size(); i > 0; i--) {
    rlt->_live_out[i - 1] -= unused_mask;
    unused_mask = unused_mask + rlt->_def[i - 1] - rlt->_use[i - 1];
    rlt->_live_in[i - 1] -= unused_mask;
  }
}

template <typename SetType>
void find_routine_params(BasicBlock* cur) {
  SetType expected = get_liveness<SetType>(cur)->_input;
  SetType provided;
  for (auto const& [_, in_node] : cur->_incoming_edges) {
    provided += get_liveness<SetType>(in_node)->_output;
  }

  if (expected - provided) {
    get_liveness<SetType>(cur)->_routine_inputs += expected - provided;
  }
}
}  // namespace

// TODO(optimize): Optionally skip FPR liveness analysis:
//  on initial disasm of the subroutine, have a marker if it has any FP instructions
//  if none, skip FP analysis and either fill _fpr_lifetimes with nothing or leave it null
void run_liveness_analysis(Subroutine& routine, BinaryContext const& ctx) {
  // Cycle 1: Evaluate liveness within a block, ignoring neighbors
  dfs_forward(
    [&ctx](BasicBlock* cur) {
      cur->_gpr_lifetimes = std::make_unique<GprLiveness>();
      cur->_fpr_lifetimes = std::make_unique<FprLiveness>();
      eval_liveness_local<GprSet>(cur, ctx);
      eval_liveness_local<FprSet>(cur, ctx);
    },
    always_iterate,
    routine._graph->_root);

  // Cycle 2: Propagate GPR/FPR liveness guesses to outputs and from inputs iteratively until there are no more changes
  bool did_change;
  do {
    did_change = false;
    dfs_forward([&did_change](BasicBlock* cur) { did_change |= propagate_guesses<GprSet>(cur); },
      always_iterate,
      routine._graph->_root);
  } while (did_change);
  do {
    did_change = false;
    dfs_forward([&did_change](BasicBlock* cur) { did_change |= propagate_guesses<FprSet>(cur); },
      always_iterate,
      routine._graph->_root);
  } while (did_change);

  // TODO(optimize): More targeted backpropagation and reverse-DFS
  // Cycle 3: Backpropagate GPR/FPR liveness guesses from outputs and to inputs into
  do {
    did_change = false;
    dfs_forward([&did_change](BasicBlock* cur) { did_change |= backpropagate_outputs<GprSet>(cur); },
      always_iterate,
      routine._graph->_root);
  } while (did_change);
  do {
    did_change = false;
    dfs_forward([&did_change](BasicBlock* cur) { did_change |= backpropagate_outputs<FprSet>(cur); },
      always_iterate,
      routine._graph->_root);
  } while (did_change);

  // Cycle 4: Clear out regions where a register is effectively dead (see comment in clear_unused_sections)
  dfs_forward(
    [](BasicBlock* cur) {
      clear_unused_sections<GprSet>(cur);
      clear_unused_sections<FprSet>(cur);
    },
    always_iterate,
    routine._graph->_root);

  // Cycle 5: Collect register-bound routine parameters into Subroutine::_gpr_param and Subroutine::_fpr_param
  dfs_forward(
    [&routine](BasicBlock* cur) {
      find_routine_params<GprSet>(cur);
      find_routine_params<FprSet>(cur);
      routine._gpr_param += cur->_gpr_lifetimes->_routine_inputs;
      routine._fpr_param += cur->_fpr_lifetimes->_routine_inputs;
    },
    always_iterate,
    routine._graph->_root);
}
}  // namespace decomp::ppc
