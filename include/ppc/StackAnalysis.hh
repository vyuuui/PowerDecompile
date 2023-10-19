#pragma once

#include <cstdint>
#include <vector>

#include "ppc/DataSource.hh"

namespace decomp::ppc {
struct SubroutineGraph;
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
  StackVariable(StackReference first, int16_t offset, TypeSet types) : _offset(offset), _types(types) {
    _refs.push_back(first);
  }
  std::vector<StackReference> _refs;
  int16_t _offset;
  TypeSet _types;
};

struct SubroutineStack {
  std::vector<StackVariable> _stack_vars;
  std::vector<StackVariable> _stack_params;
  uint16_t _stack_size;
};

void run_stack_analysis(SubroutineGraph const& graph, SubroutineStack& out_stack);
}  // namespace decomp::ppc
