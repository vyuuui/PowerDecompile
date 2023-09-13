#pragma once

#include <string>

#include "DataSource.hh"
#include "PpcDisasm.hh"

namespace decomp {
std::string opcode_name(InstOperation op);
std::string datasource_verbose(DataSource const& src);
std::string immsource_verbose(ImmSource const& src);
std::string datasource_disasm_generic(DataSource const& src);
std::string immsource_disasm_generic(ImmSource const& src);

enum class CRBitDisasmFormat {
  SingleBit, Field, Mask
};
std::string disasm_crbit(CRBit crbits, CRBitDisasmFormat format);
std::string disasm_fpscrbit(FPSCRBit fpscrbits, CRBitDisasmFormat format);

}
