#pragma once

#include <ostream>

namespace decomp {
struct MetaInst;

void write_disassembly(MetaInst const& disasm, std::ostream& sink);
}  // namespace decomp
