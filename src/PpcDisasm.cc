#include <cstddef>
#include <cstdint>

#include <iostream>

#include "DataSource.hh"
#include "PpcDisasm.hh"
#include "ReservedVector.hh"

namespace {
void disasm_paired_single(BinInst binst, MetaInst& meta_out) {

}
}

void disasm_single(BinInst binst, MetaInst& meta_out) {
  switch (binst._opcode) {
    case 1:
      break;
    case 2:
      disasm_paired_single(binst, meta_out);
      break;
  }
}
