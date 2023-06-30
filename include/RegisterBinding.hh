#pragma once

#include <cstdint>
#include <variant>
#include <vector>

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

  constexpr void operator+=(T reg) { _set |= 1 << static_cast<uint8_t>(reg); }
  constexpr void operator+=(RegSet s) { _set |= s._set; }

  constexpr void operator-=(T reg) { _set &= ~(1 << static_cast<uint8_t>(reg)); }
  constexpr void operator-=(RegSet s) { _set &= ~s._set; }

  constexpr RegSet operator+(RegSet rhs) const { return _set | rhs._set; }
  constexpr RegSet operator-(RegSet rhs) const { return _set & ~rhs._set; }
};

struct RegisterLifetimes : public BlockPrivate {
  // Registers alive going into X instruction
  std::vector<RegSet<GPR>> _live_in;
  // Registers alive going out of X instruction
  std::vector<RegSet<GPR>> _live_out;
  // Registers scrapped by this instruction
  std::vector<RegSet<GPR>> _kill;

  RegSet<GPR> _input;
  RegSet<GPR> _output;
  virtual ~RegisterLifetimes() {}
};

void evaluate_bindings(RandomAccessData const& ram, SubroutineGraph& graph);

}  // namespace decomp
