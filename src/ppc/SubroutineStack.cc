#include "ppc/SubroutineStack.hh"

#include <algorithm>
#include <optional>
#include <variant>

#include "ppc/DataSource.hh"
#include "ppc/PpcDisasm.hh"
#include "ppc/Subroutine.hh"
#include "ppc/SubroutineGraph.hh"
#include "utl/ReservedVector.hh"
#include "utl/VariantOverloaded.hh"

namespace decomp::ppc {
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
}  // namespace

bool SubroutineStack::in_param_region(int16_t offset) const { return offset > _stack_size + 4; }

void SubroutineStack::analyze_block(BasicBlock const* block) {
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
          analyze_sp_mem_read(inst);
          break;

        case Write:
          analyze_sp_mem_write(inst);
          break;

        case Reference:
          analyze_sp_mem_ref(inst);
          break;

        case SpModify:
          analyze_sp_modify(inst);
          break;
      }
    }
  }
}

void SubroutineStack::analyze_readwrite(MetaInst const& inst, MemRegOff operand, ReferenceType reftype) {
  auto type = operand._type;
  reserved_vector<int16_t, 2> offsets;
  offsets.push_back(abs(operand._offset));
  if (operand._type == DataType::kPackedSingle) {
    offsets.push_back(abs(operand._offset) + 4);
    type = DataType::kSingle;
  }

  for (int16_t offset : offsets) {
    const bool is_param = in_param_region(offset);
    std::vector<StackVariable>& region = is_param ? _stack_params : _stack_vars;

    auto it =
      std::find_if(region.begin(), region.end(), [offset](StackVariable const& var) { return var._offset == offset; });
    if (it == region.end()) {
      region.emplace_back(StackReference(inst._va, reftype), offset, convert_data_type(type), is_param);
    } else {
      it->_refs.emplace_back(inst._va, reftype);
      it->_types = it->_types | convert_data_type(type);
    }
  }
}

void SubroutineStack::analyze_sp_mem_read(MetaInst const& inst) {
  analyze_readwrite(inst, inst.get_read_op<MemRegOff>(), ReferenceType::kRead);
}

void SubroutineStack::analyze_sp_mem_write(MetaInst const& inst) {
  analyze_readwrite(inst, inst.get_write_op<MemRegOff>(), ReferenceType::kWrite);
}

void SubroutineStack::analyze_sp_mem_ref(MetaInst const& inst) {
  if (inst._op == InstOperation::kAddi || inst._op == InstOperation::kAddic) {
    int16_t offset = std::get<SIMM>(inst._reads[1])._imm_value;

    const bool is_param = in_param_region(offset);
    std::vector<StackVariable>& region = is_param ? _stack_params : _stack_vars;

    auto it =
      std::find_if(region.begin(), region.end(), [offset](StackVariable const& var) { return var._offset == offset; });
    if (it == region.end()) {
      region.emplace_back(StackReference(inst._va, ReferenceType::kAddress), offset, TypeSet::kNone, is_param);
    } else {
      it->_refs.emplace_back(inst._va, ReferenceType::kAddress);
    }
  }
  // TODO: other forms of referencing?
}

void SubroutineStack::analyze_sp_modify(MetaInst const& inst) {
  if (inst._op == InstOperation::kAddi) {
    // TODO: stash the stack restore location somewhere
  } else if (inst._op == InstOperation::kStwu) {
    _stack_size = static_cast<uint16_t>(-inst.get_write_op<MemRegOff>()._offset);
  }
}

void SubroutineStack::analyze(SubroutineGraph const& graph) {
  dfs_forward([this](BasicBlock const* cur) { analyze_block(cur); }, always_iterate, graph._root);
}

StackVariable const* SubroutineStack::variable_for_offset(int16_t offset) const {
  std::vector<StackVariable> const& region = in_param_region(offset) ? _stack_params : _stack_vars;
  auto it =
    std::find_if(region.begin(), region.end(), [offset](StackVariable const& var) { return var._offset == offset; });

  return it == region.end() ? nullptr : &*it;
}

StackVariable* SubroutineStack::variable_for_offset(int16_t offset) {
  return const_cast<StackVariable*>(const_cast<SubroutineStack const*>(this)->variable_for_offset(offset));
}

void run_stack_analysis(Subroutine& routine) {
  routine._stack = std::make_unique<SubroutineStack>();
  routine._stack->analyze(*routine._graph);
}
}  // namespace decomp::ppc
