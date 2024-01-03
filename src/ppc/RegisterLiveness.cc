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
  } else if constexpr (std::is_same_v<SetType, CrSet>) {
    if (std::holds_alternative<CrSlice>(read)) {
      uses += std::get<CrSlice>(read)._field;
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
  } else if constexpr (std::is_same_v<SetType, CrSet>) {
    if (std::holds_alternative<CrSlice>(write)) {
      defs += std::get<CrSlice>(write)._field;
    }
  }
  return defs;
}

template <typename SetType>
SetType defs_for_flags(InstFlags flags) {
  SetType defs;
  if constexpr (std::is_same_v<SetType, CrSet>) {
    if (check_flags(flags, InstFlags::kWritesRecord)) {
      defs += CRField::kCr0;
    }
    if (check_flags(flags, InstFlags::kWritesFpRecord)) {
      defs += CRField::kCr1;
    }
  }
  return defs;
}

template <typename SetType>
void eval_liveness_local(BasicBlock& block, BinaryContext const& ctx) {
  auto rlt = get_liveness<SetType>(&block);

  SetType inputs;
  SetType outputs;
  SetType def_mask;

  for (size_t i = 0; i < block._instructions.size(); i++) {
    MetaInst const& inst = block._instructions[i];

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
      def += defs_for_flags<SetType>(inst._flags);
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
  for (size_t i = 0; input_mask && i < block._instructions.size(); i++) {
    rlt->_live_in[i] += input_mask;
    input_mask -= rlt->_use[i];
    rlt->_live_out[i] += input_mask;
  }
}

template <typename SetType>
bool backpropagate_outputs(SubroutineGraph& graph, BasicBlockVertex& bbv) {
  auto rlt = get_liveness<SetType>(&bbv.data());

  SetType outedge_inputs;
  graph.foreach_real_outedge(
    [&outedge_inputs](BasicBlockVertex& bbv) { outedge_inputs += get_liveness<SetType>(&bbv.data())->_input; }, &bbv);
  if (graph.is_exit_vertex(&bbv)) {
    outedge_inputs += std::get<SetType>(kRegSets._return_set);
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
    for (size_t i = 0; i < bbv.data()._instructions.size(); i++) {
      rlt->_live_in[i] += used_pt;
      rlt->_live_out[i] += used_pt;
    }
    return true;
  }

  return false;
}

template <typename SetType>
bool propagate_guesses(SubroutineGraph& graph, BasicBlockVertex& bbv) {
  auto rlt = get_liveness<SetType>(&bbv.data());

  SetType passthrough_inputs;
  graph.foreach_real_inedge(
    [&passthrough_inputs](BasicBlockVertex& bbv) {
      passthrough_inputs +=
        get_liveness<SetType>(&bbv.data())->_guess_out + get_liveness<SetType>(&bbv.data())->_propagated;
    },
    &bbv);

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
void clear_unused_sections(BasicBlock& block, BinaryContext const& ctx) {
  auto rlt = get_liveness<SetType>(&block);

  // Sweep through the block to clear out liveness for unused sections, E.G.
  // D=Def U=Use .=Neither
  // .....D.............U.........U............D......U.....
  //                              |____________|
  //                              unused section
  SetType unused_mask = rlt->_guess_out;
  for (size_t i = block._instructions.size(); i > 0; i--) {
    MetaInst const& inst = block._instructions[i - 1];
    rlt->_live_out[i - 1] -= unused_mask;

    SetType possible_call_uses;
    if (check_flags(inst._flags, InstFlags::kWritesLR)) {
      if (inst._op == InstOperation::kB && !is_abi_routine(ctx, inst.branch_target())) {
        possible_call_uses = std::get<SetType>(kRegSets._parameter_set);
      } else if (inst._op == InstOperation::kBclr || inst._op == InstOperation::kBc ||
                 inst._op == InstOperation::kBcctr) {
        possible_call_uses = std::get<SetType>(kRegSets._parameter_set);
      }
    }
    unused_mask = unused_mask + rlt->_def[i - 1] - rlt->_use[i - 1] - possible_call_uses;
    rlt->_live_in[i - 1] -= unused_mask;
  }
}

template <typename SetType>
void find_routine_params(SubroutineGraph& graph, BasicBlockVertex& bbv) {
  SetType expected = get_liveness<SetType>(&bbv.data())->_input;
  SetType provided;
  graph.foreach_real_inedge(
    [&provided](BasicBlockVertex& bbv) { provided += get_liveness<SetType>(&bbv.data())->_output; }, &bbv);

  get_liveness<SetType>(&bbv.data())->_routine_inputs +=
    (expected - provided) & std::get<SetType>(kRegSets._parameter_set);
}
}  // namespace

// TODO(optimize): Optionally skip FPR liveness analysis:
//  on initial disasm of the subroutine, have a marker if it has any FP instructions
//  if none, skip FP analysis and either fill _fpr_lifetimes with nothing or leave it null
void run_liveness_analysis(Subroutine& routine, BinaryContext const& ctx) {
  struct IterState {
    bool gpr_changed = false, gpr_done = false;
    bool fpr_changed = false, fpr_done = false;
    bool cr_changed = false, cr_done = false;
    void iterate() {
      gpr_done = !gpr_changed;
      fpr_done = !fpr_changed;
      cr_done = !cr_changed;
      gpr_changed = false;
      fpr_changed = false;
      cr_changed = false;
    }
    bool done() const { return gpr_done && fpr_done && cr_done; }
  };

  // Cycle 1: Evaluate liveness within a block, ignoring neighbors
  routine._graph->foreach_real([&ctx](BasicBlockVertex& bbv) {
    bbv.data()._gpr_lifetimes = std::make_unique<GprLiveness>();
    bbv.data()._fpr_lifetimes = std::make_unique<FprLiveness>();
    bbv.data()._cr_lifetimes = std::make_unique<CrLiveness>();
    eval_liveness_local<GprSet>(bbv.data(), ctx);
    eval_liveness_local<FprSet>(bbv.data(), ctx);
    eval_liveness_local<CrSet>(bbv.data(), ctx);
  });

  // Cycle 2: Propagate GPR/FPR liveness guesses to outputs and from inputs iteratively until there are no more changes
  for (IterState iter_state; !iter_state.done(); iter_state.iterate()) {
    routine._graph->preorder_fwd(
      [&iter_state, &routine](BasicBlockVertex& bbv) {
        if (!bbv.is_real()) {
          return;
        }

        if (!iter_state.gpr_done) {
          iter_state.gpr_changed |= propagate_guesses<GprSet>(*routine._graph, bbv);
        }
        if (!iter_state.fpr_done) {
          iter_state.fpr_changed |= propagate_guesses<FprSet>(*routine._graph, bbv);
        }
        if (!iter_state.cr_done) {
          iter_state.cr_changed |= propagate_guesses<CrSet>(*routine._graph, bbv);
        }
      },
      routine._graph->root());
  }

  // Cycle 3: Backpropagate GPR/FPR liveness guesses from outputs and to inputs into
  for (IterState iter_state; !iter_state.done(); iter_state.iterate()) {
    routine._graph->postorder_fwd(
      [&iter_state, &routine](BasicBlockVertex& bbv) {
        if (!bbv.is_real()) {
          return;
        }

        if (!iter_state.gpr_done) {
          iter_state.gpr_changed |= backpropagate_outputs<GprSet>(*routine._graph, bbv);
        }
        if (!iter_state.fpr_done) {
          iter_state.fpr_changed |= backpropagate_outputs<FprSet>(*routine._graph, bbv);
        }
        if (!iter_state.cr_done) {
          iter_state.cr_changed |= backpropagate_outputs<CrSet>(*routine._graph, bbv);
        }
      },
      routine._graph->root());
  }

  // Cycle 4: Clear out regions where a register is effectively dead (see comment in clear_unused_sections)
  routine._graph->foreach_real([&ctx](BasicBlockVertex& bbv) {
    clear_unused_sections<GprSet>(bbv.data(), ctx);
    clear_unused_sections<FprSet>(bbv.data(), ctx);
    clear_unused_sections<CrSet>(bbv.data(), ctx);
  });

  // Cycle 5: Collect register-bound routine parameters into Subroutine::_gpr_param and Subroutine::_fpr_param
  routine._graph->foreach_real([&routine](BasicBlockVertex& bbv) {
    find_routine_params<GprSet>(*routine._graph, bbv);
    find_routine_params<FprSet>(*routine._graph, bbv);
    routine._gpr_param += bbv.data()._gpr_lifetimes->_routine_inputs;
    routine._fpr_param += bbv.data()._fpr_lifetimes->_routine_inputs;
    // TODO: Check if CR parameters are some kind of standard
  });
}
}  // namespace decomp::ppc
