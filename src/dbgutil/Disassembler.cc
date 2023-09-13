#include "dbgutil/Disassembler.hh"

#include <fmt/format.h>

#include <ostream>

#include "PpcDisasm.hh"
#include "dbgutil/DbgUtilShared.hh"

namespace decomp {
void write_inst_disassembly(const MetaInst& inst, std::ostream& sink) {
  // TODO
}

void write_inst_info(MetaInst const& inst, std::ostream& sink) {
  if (inst._op == InstOperation::kInvalid) {
    sink << "Invalid decode" << std::endl;
    return;
  }

  auto print_list = [&sink](auto const& list, auto cb) {
    if (list.empty()) {
      return;
    }
    sink << cb(list[0]);
    for (size_t i = 1; i < list.size(); i++) {
      sink << ", " << cb(list[i]);
    }
  };

  sink << fmt::format("Binst: {:08x}", inst._binst);
  sink << "\nOperand: " << opcode_name(inst._op);
  sink << "\nReads: ";
  print_list(inst._reads, datasource_verbose);

  sink << "\nImmediates: ";
  print_list(inst._immediates, immsource_verbose);

  sink << "\nWrites: ";
  print_list(inst._writes, datasource_verbose);

  sink << fmt::format("\nFlags: {:b}", static_cast<uint32_t>(inst._flags));

  sink << std::endl;
}
}  // namespace decomp
