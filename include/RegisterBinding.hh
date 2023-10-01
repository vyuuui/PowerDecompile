#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "BinaryContext.hh"
#include "DataSource.hh"
#include "RegSet.hh"
#include "SubroutineGraph.hh"

namespace decomp {

class RandomAccessData;
struct SubroutineGraph;

struct RegisterLifetimes : public BlockPrivate {
  // Registers (re)defined by this instruction
  std::vector<RegSet<GPR>> _def;
  // Registers referenced by this instruction
  std::vector<RegSet<GPR>> _use;
  // Registers scrapped by this instruction
  std::vector<RegSet<GPR>> _kill;
  // Registers alive going into this instruction
  std::vector<RegSet<GPR>> _live_in;
  // Registers alive going out of this instruction
  std::vector<RegSet<GPR>> _live_out;

  RegSet<GPR> _input;
  RegSet<GPR> _output;
  // Set of all registers that are overwritten by this block
  RegSet<GPR> _overwritten;
  RegSet<GPR> _killed_at_entry;
  // Set of registers that could be the result of a return
  RegSet<GPR> _untouched_retval;
  RegSet<GPR> _output_retval;
  std::optional<size_t> _last_call_index;
  virtual ~RegisterLifetimes() {}
};

void evaluate_bindings(SubroutineGraph& graph, BinaryContext const& ctx);

}  // namespace decomp
