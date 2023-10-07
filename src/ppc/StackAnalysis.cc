#include "ppc/StackAnalysis.hh"

#include <algorithm>
#include <optional>
#include <variant>

#include "ppc/DataSource.hh"
#include "ppc/PpcDisasm.hh"
#include "ppc/SubroutineGraph.hh"
#include "utl/ReservedVector.hh"
#include "utl/VariantOverloaded.hh"

namespace decomp {
namespace {
TypeSet convert_data_type(DataType type) {
  switch (type) {
    case DataType::kS1:
      return TypeSet::kByte;

    case DataType::kS2:
      return TypeSet::kHalfWord;

    case DataType::kS4:
      return TypeSet::kWord;

    case DataType::kSingle:
      return TypeSet::kSingle;

    case DataType::kDouble:
      return TypeSet::kDouble;

    default:
      return TypeSet::kNone;
  }
}

std::vector<StackVariable>& region_for_offset(SubroutineStack& stack, int16_t offset) {
  return offset > stack._stack_size + 4 ? stack._stack_params : stack._stack_vars;
}

void analyze_readwrite(MetaInst const& inst, SubroutineStack& out_stack, MemRegOff operand, ReferenceType reftype) {
  auto type = operand._type;
  reserved_vector<int16_t, 2> offsets;
  offsets.push_back(abs(operand._offset));
  if (operand._type == DataType::kPackedSingle) {
    offsets.push_back(abs(operand._offset) + 4);
    type = DataType::kSingle;
  }

  for (int16_t offset : offsets) {
    std::vector<StackVariable>& region = region_for_offset(out_stack, offset);
    auto it =
      std::find_if(region.begin(), region.end(), [offset](StackVariable const& var) { return var._offset == offset; });
    if (it == region.end()) {
      region.emplace_back(StackReference(inst._va, reftype), offset, convert_data_type(type));
    } else {
      it->_refs.emplace_back(inst._va, reftype);
      it->_types = it->_types | convert_data_type(type);
    }
  }
}

void analyze_sp_mem_read(MetaInst const& inst, SubroutineStack& out_stack) {
  analyze_readwrite(inst, out_stack, inst.get_read_op<MemRegOff>(), ReferenceType::kRead);
}

void analyze_sp_mem_write(MetaInst const& inst, SubroutineStack& out_stack) {
  analyze_readwrite(inst, out_stack, inst.get_write_op<MemRegOff>(), ReferenceType::kWrite);
}

void analyze_sp_mem_ref(MetaInst const& inst, SubroutineStack& out_stack) {
  if (inst._op == InstOperation::kAddi) {
    int16_t offset = inst.get_imm_op<SIMM>()._imm_value;
    std::vector<StackVariable>& region = region_for_offset(out_stack, offset);
    auto it =
      std::find_if(region.begin(), region.end(), [offset](StackVariable const& var) { return var._offset == offset; });
    if (it == region.end()) {
      region.emplace_back(StackReference(inst._va, ReferenceType::kAddress), offset, TypeSet::kNone);
    } else {
      it->_refs.emplace_back(inst._va, ReferenceType::kAddress);
    }
  }
  // TODO: other forms of referencing?
}

void analyze_sp_modify(MetaInst const& inst, SubroutineStack& out_stack) {
  if (inst._op == InstOperation::kAddi) {
    // TODO: stash the stack restore location somewhere
  } else if (inst._op == InstOperation::kStwu) {
    out_stack._stack_size = static_cast<uint16_t>(-inst.get_write_op<MemRegOff>()._offset);
  }
}

void analyze_block(BasicBlock const* block, SubroutineStack& out_stack) {
  enum SPReferenceType { Write, Read, Reference, SpModify };
  for (MetaInst const& inst : block->_instructions) {
    // There can't be more than 3 references to a register in a single instruction
    reserved_vector<SPReferenceType, 3> all_sp_refs;

    for (auto& read : inst._reads) {
      auto ref =
        std::visit(overloaded{
                     [](MemRegOff mem) { return mem._base == GPR::kR1 ? std::make_optional(Read) : std::nullopt; },
                     [](GPRSlice r) { return r._reg == GPR::kR1 ? std::make_optional(Reference) : std::nullopt; },
                     [](auto) { return std::optional<SPReferenceType>{std::nullopt}; },
                   },
          read);
      if (ref) {
        all_sp_refs.emplace_back(*ref);
      }
    }

    for (auto& write : inst._writes) {
      auto ref =
        std::visit(overloaded{
                     [](MemRegOff mem) { return mem._base == GPR::kR1 ? std::make_optional(Write) : std::nullopt; },
                     [](GPRSlice r) { return r._reg == GPR::kR1 ? std::make_optional(SpModify) : std::nullopt; },
                     [](auto) { return std::optional<SPReferenceType>{std::nullopt}; },
                   },
          write);
      if (ref) {
        all_sp_refs.emplace_back(*ref);
      }
    }

    // After collecting all references to the SP, determine what action(s) this instruction is doing to the stack
    for (SPReferenceType reftype : all_sp_refs) {
      switch (reftype) {
        case Read:
          analyze_sp_mem_read(inst, out_stack);
          break;

        case Write:
          analyze_sp_mem_write(inst, out_stack);
          break;

        case Reference:
          analyze_sp_mem_ref(inst, out_stack);
          break;

        case SpModify:
          analyze_sp_modify(inst, out_stack);
          break;
      }
    }
  }
}
}  // namespace

void run_stack_analysis(SubroutineGraph const& graph, SubroutineStack& stack_out) {
  dfs_forward([&stack_out](BasicBlock const* cur) { analyze_block(cur, stack_out); }, always_iterate, graph._root);
}
}  // namespace decomp
