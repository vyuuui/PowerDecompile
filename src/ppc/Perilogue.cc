#include "ppc/Perilogue.hh"

#include "ppc/BinaryContext.hh"
#include "ppc/PpcDisasm.hh"
#include "ppc/RegisterLiveness.hh"
#include "ppc/Subroutine.hh"
#include "ppc/SubroutineGraph.hh"
#include "ppc/SubroutineStack.hh"

namespace decomp::ppc {
namespace {
void perilogue_block_analysis(BasicBlock* block, SubroutineStack& stack, BinaryContext const& ctx) {
  constexpr auto backtrack_calle_save = [](BasicBlock const* block, GPR reg, size_t start) {
    size_t j;
    for (j = start; j > 0; j--) {
      if (!block->_block_lifetimes->_live_out[j - 1].in_set(reg)) {
        break;
      }
    }
    if (j == 0) {
      return PerilogueInstructionType::kCalleeGPRSave;
    }

    return PerilogueInstructionType::kNormalInst;
  };

  for (size_t i = 0; i < block->_instructions.size(); i++) {
    MetaInst const& inst = block->_instructions[i];
    PerilogueInstructionType itype = PerilogueInstructionType::kNormalInst;

    if (inst._op == InstOperation::kStwu && inst._binst.rs() == GPR::kR1) {
      itype = PerilogueInstructionType::kFrameAllocate;
    } else if (inst._op == InstOperation::kAddi && inst._binst.rd() == GPR::kR1 && inst._binst.ra() == GPR::kR1) {
      itype = PerilogueInstructionType::kFrameDeallocate;
    } else if (inst._op == InstOperation::kMfspr && inst._binst.rd() == GPR::kR0 && inst._binst.spr() == SPR::kLr) {
      itype = PerilogueInstructionType::kMoveLRToR0;
      // TODO: improve this this by ensuring that LR is a routine input
      // (requires liveness tracking of SPRs)
    } else if (inst._op == InstOperation::kMtspr && inst._binst.rs() == GPR::kR0 && inst._binst.spr() == SPR::kLr) {
      for (size_t j = i; j > 0; j--) {
        if (!block->_block_lifetimes->_live_out[j - 1].in_set(GPR::kR0)) {
          break;
        }
        if (block->_perilogue_types[j - 1] == PerilogueInstructionType::kLoadSenderLR) {
          itype = PerilogueInstructionType::kMoveR0toLR;
          break;
        }
      }
    } else if (inst._op == InstOperation::kStw && inst._binst.ra() == GPR::kR1) {
      MemRegOff store_loc = std::get<MemRegOff>(inst._writes[0]);
      GPR store_reg = std::get<GPRSlice>(inst._reads[0])._reg;

      if (store_reg == GPR::kR0) {
        // I really hope that LR saves can't happen across basic blocks
        for (size_t j = i; j > 0; j--) {
          if (!block->_block_lifetimes->_live_out[j - 1].in_set(store_reg)) {
            break;
          }
          if (block->_perilogue_types[j - 1] == PerilogueInstructionType::kMoveLRToR0) {
            itype = PerilogueInstructionType::kSaveSenderLR;
            stack.variable_for_offset(store_loc._offset)->_is_frame_storage = true;
            break;
          }
        }

      } else if (kCalleeSavedGpr.in_set(store_reg)) {
        itype = backtrack_calle_save(block, store_reg, i);

        if (itype == PerilogueInstructionType::kCalleeGPRSave) {
          stack.variable_for_offset(store_loc._offset)->_is_frame_storage = true;
        }
      }
    } else if (inst._op == InstOperation::kStmw && inst._binst.ra() == GPR::kR1) {
      MemRegOff store_loc = std::get<MemRegOff>(inst._writes[0]);

      itype = backtrack_calle_save(block, std::get<MultiReg>(inst._reads[0])._low, i);

      if (itype == PerilogueInstructionType::kCalleeGPRSave) {
        stack.variable_for_offset(store_loc._offset)->_is_frame_storage = true;
      }
    } else if (inst._op == InstOperation::kLwz && inst._binst.ra() == GPR::kR1) {
      MemRegOff read_loc = std::get<MemRegOff>(inst._reads[0]);
      GPR read_reg = std::get<GPRSlice>(inst._writes[0])._reg;
      if (stack.variable_for_offset(read_loc._offset)->_is_frame_storage) {
        if (read_reg == GPR::kR0) {
          itype = PerilogueInstructionType::kLoadSenderLR;
        } else if (kCalleeSavedGpr.in_set(read_reg)) {
          itype = PerilogueInstructionType::kLoadSenderLR;
        }
      }
    } else if (inst._op == InstOperation::kLmw && inst._binst.ra() == GPR::kR1) {
      MemRegOff read_loc = std::get<MemRegOff>(inst._reads[0]);
      GPR read_reg = std::get<MultiReg>(inst._writes[0])._low;
      if (stack.variable_for_offset(read_loc._offset)->_is_frame_storage) {
        if (read_reg == GPR::kR0) {
          itype = PerilogueInstructionType::kLoadSenderLR;
        } else if (kCalleeSavedGpr.in_set(read_reg)) {
          itype = PerilogueInstructionType::kLoadSenderLR;
        }
      }
    } else if (inst._op == InstOperation::kB && is_abi_routine(ctx, inst.branch_target())) {
      itype = PerilogueInstructionType::kAbiRoutine;
      assert(i > 0);
      MetaInst const& load_base_inst = block->_instructions[i - 1];
      if (load_base_inst._op == InstOperation::kAddi &&
          std::get<GPRSlice>(load_base_inst._writes[0])._reg == GPR::kR11 &&
          std::get<GPRSlice>(load_base_inst._reads[0])._reg == GPR::kR1) {
        block->_perilogue_types[i - 1] = PerilogueInstructionType::kCalleeGPRSave;
        stack.variable_for_offset(std::get<SIMM>(load_base_inst._reads[1])._imm_value)->_is_frame_storage = true;
      }
    }
    // TODO: floating point analysis

    block->_perilogue_types[i] = itype;
  }
}
}  // namespace

void run_perilogue_analysis(Subroutine& routine, BinaryContext const& ctx) {
  dfs_forward([](BasicBlock* cur) { cur->_perilogue_types.resize(cur->_instructions.size()); },
    always_iterate,
    routine._graph._root);
  perilogue_block_analysis(routine._graph._root, routine._stack, ctx);
  for (BasicBlock* terminal_block : routine._graph._exit_points) {
    perilogue_block_analysis(terminal_block, routine._stack, ctx);
  }
}
}  // namespace decomp::ppc
