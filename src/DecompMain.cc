#include "PpcDisasm.hh"

#include <iostream>
#include <string>

#include "DisasmWrite.hh"

int main() {
  using namespace decomp;

  MetaInst disasm;
  uint32_t raw_inst = 0x4182fff8;

  disasm_single(raw_inst, disasm);

  write_disassembly(disasm, std::cout);

  return 0;
}
