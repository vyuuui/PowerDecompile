#pragma once

#include <cstdint>
#include <ostream>

namespace decomp {
struct MetaInst;

void write_inst_disassembly(MetaInst const& inst, std::ostream& sink, uint32_t source_address);
void write_inst_info(MetaInst const& disasm, std::ostream& sink);
}
