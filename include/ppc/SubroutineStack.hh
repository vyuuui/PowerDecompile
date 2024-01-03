#pragma once

#include <cstdint>
#include <vector>

#include "ppc/DataSource.hh"

namespace decomp::ppc {
struct BasicBlock;
struct MetaInst;
struct Subroutine;
class SubroutineGraph;

enum class TypeSet : uint8_t {
  kNone = 0,
  kAll = 0b11111,

  kByte = 1u << 0,
  kHalfWord = 1u << 1,
  kWord = 1u << 2,
  kSingle = 1u << 3,
  kDouble = 1u << 4,
};
GEN_FLAG_OPERATORS(TypeSet);

enum class ReferenceType {
  kRead,
  kWrite,
  kAddress,
};

struct StackReference {
  StackReference(uint32_t location, ReferenceType reftype) : _location(location), _reftype(reftype) {}
  uint32_t _location;
  ReferenceType _reftype;
};

struct StackVariable {
  StackVariable(StackReference first, int16_t offset, TypeSet types, bool is_param)
      : _offset(offset), _types(types), _is_param(is_param), _is_frame_storage(false) {
    _refs.push_back(first);
  }
  std::vector<StackReference> _refs;
  int16_t _offset;
  TypeSet _types;
  bool _is_param;
  bool _is_frame_storage;
};

class SubroutineStack {
private:
  std::vector<StackVariable> _stack_vars;
  std::vector<StackVariable> _stack_params;
  uint16_t _stack_size;

private:
  bool in_param_region(int16_t offset) const;
  void analyze_block(BasicBlock const& block);

  void analyze_readwrite(MetaInst const& inst, MemRegOff operand, ReferenceType reftype);

  void analyze_sp_mem_read(MetaInst const& inst);
  void analyze_sp_mem_write(MetaInst const& inst);
  void analyze_sp_mem_ref(MetaInst const& inst);
  void analyze_sp_modify(MetaInst const& inst);

  void analyze(SubroutineGraph const& graph);

  friend void run_stack_analysis(Subroutine& routine);

public:
  StackVariable const* variable_for_offset(int16_t offset) const;
  StackVariable* variable_for_offset(int16_t offset);
  constexpr uint16_t stack_size() const { return _stack_size; }

  std::vector<StackVariable> const& var_list() const { return _stack_vars; }
  std::vector<StackVariable> const& param_list() const { return _stack_params; }
};

void run_stack_analysis(Subroutine& routine);
}  // namespace decomp::ppc
