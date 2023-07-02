#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "BinaryContext.hh"
#include "DataSource.hh"
#include "SubroutineGraph.hh"

namespace decomp {

class RandomAccessData;
struct SubroutineGraph;

struct BlockCoverage {
  uint32_t _block_id;
  uint16_t _low_off;
  uint16_t _high_off;
};

struct TempBinding {
  uint32_t _temp_id;
  std::variant<GPR, FPR> _register;
  std::vector<BlockCoverage> _coverage_list;
};

template <typename T>
struct RegSet {
  uint32_t _set;
  constexpr RegSet() : _set(0) {}
  constexpr RegSet(uint32_t s) : _set(s) {}

  constexpr bool in_set(T reg) const { return ((1 << static_cast<uint8_t>(reg)) & _set) != 0; }
  constexpr bool empty() const { return _set == 0; }
  constexpr bool operator==(RegSet rhs) const { return _set == rhs._set; }

  // Add register to set
  constexpr void operator+=(T reg) { _set |= 1 << static_cast<uint8_t>(reg); }
  // Set union
  constexpr void operator+=(RegSet s) { _set |= s._set; }

  // Remove register from set
  constexpr void operator-=(T reg) { _set &= ~(1 << static_cast<uint8_t>(reg)); }
  // Set difference
  constexpr void operator-=(RegSet s) { _set &= ~s._set; }

  // Set intersection
  constexpr void operator&=(RegSet s) { _set &= s._set; }

  // Set union
  constexpr RegSet operator+(RegSet rhs) const { return _set | rhs._set; }
  // Set difference
  constexpr RegSet operator-(RegSet rhs) const { return _set & ~rhs._set; }
  // Set intersection
  constexpr RegSet operator&(RegSet rhs) const { return _set & rhs._set; }
};

struct RegisterLifetimes : public BlockPrivate {
  // Registers (re)defined by this instruction
  std::vector<RegSet<GPR>> _def;
  // Registers (re)defined by this instruction
  std::vector<RegSet<GPR>> _use;
  // Registers scrapped by this instruction
  std::vector<RegSet<GPR>> _kill;
  // Registers alive going into X instruction
  std::vector<RegSet<GPR>> _live_in;
  // Registers alive going out of X instruction
  std::vector<RegSet<GPR>> _live_out;

  RegSet<GPR> _input;
  RegSet<GPR> _output;
  // Set of all registers that are overwritten by this block
  RegSet<GPR> _overwritten;
  RegSet<GPR> _killed_at_entry;
  virtual ~RegisterLifetimes() {}
};

void evaluate_bindings(SubroutineGraph& graph, BinaryContext const& ctx);

}  // namespace decomp
