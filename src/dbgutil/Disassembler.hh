#pragma once

#include <cstdint>
#include <ostream>

#include "ppc/PpcDisasm.hh"

namespace decomp {
void write_inst_disassembly(ppc::MetaInst const& inst, std::ostream& sink);
void write_inst_info(ppc::MetaInst const& disasm, std::ostream& sink);
}  // namespace decomp
