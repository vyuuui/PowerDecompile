#pragma once

#include <ostream>

#include "ir/GekkoTranslator.hh"
#include "ir/IrInst.hh"

namespace decomp {
void write_ir_inst(ir::IrInst const& inst, std::ostream& sink);
void write_block_transition(ir::BlockTransition const& transition, std::ostream& sink);
void write_block(ir::IrBlock const& block, std::ostream& sink);
}  // namespace decomp
