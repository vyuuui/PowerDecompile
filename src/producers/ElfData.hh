#pragma once

#include "producers/SectionedData.hh"

namespace decomp {
struct ElfSection {
  ElfSection(uint32_t fo, uint32_t va, uint32_t sz) : _file_off(fo), _vaddr(va), _size(sz) {}
  ElfSection() {}
  uint32_t _file_off = 0, _vaddr = 0, _size = 0;
};

class ElfData : public SectionedData {
private:
  std::vector<ElfSection> _text_sections;
  std::vector<ElfSection> _data_sections;
  uint32_t _entrypoint;

public:
  bool load_from(std::istream& source);

  std::vector<ElfSection> const& text_section_headers() const { return _text_sections; }
  std::vector<ElfSection> const& data_section_headers() const { return _data_sections; }
  uint32_t entrypoint() const { return _entrypoint; }

  virtual ~ElfData() {}
};
}
