#pragma once

#include <ostream>

namespace decomp {
struct MetaInst;

void write_inst_disassembly(MetaInst const& inst, std::ostream& sink);
void write_inst_info(MetaInst const& disasm, std::ostream& sink);
}
