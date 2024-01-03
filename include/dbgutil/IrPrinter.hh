#pragma once

#include <ostream>

#include "ir/GekkoTranslator.hh"
#include "ir/IrInst.hh"

namespace decomp {
void write_ir_inst(ir::IrInst const& inst, std::ostream& sink);
void write_block(ir::IrBlockVertex const& block, std::ostream& sink);
}  // namespace decomp
