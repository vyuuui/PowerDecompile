#pragma once

#include <cstdint>

#include "ppc/PpcDisasm.hh"

namespace decomp {

class RandomAccessData {
public:
  virtual ~RandomAccessData() {}

  virtual uint8_t read_byte(uint32_t vaddr) const = 0;
  virtual uint16_t read_half(uint32_t vaddr) const = 0;
  virtual uint32_t read_word(uint32_t vaddr) const = 0;
  virtual uint64_t read_long(uint32_t vaddr) const = 0;
  virtual float read_float(uint32_t vaddr) const = 0;
  virtual double read_double(uint32_t vaddr) const = 0;

  MetaInst read_instruction(uint32_t vaddr) const {
    MetaInst ret;
    disasm_single(vaddr, read_word(vaddr), ret);
    return ret;
  }
};

}  // namespace decomp
