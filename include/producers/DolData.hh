#pragma once

#include <array>

#include "ReservedVector.hh"
#include "producers/SectionedData.hh"

namespace decomp {
struct DolSection {
  DolSection(uint32_t fo, uint32_t va, uint32_t sz) : _file_off(fo), _vaddr(va), _size(sz) {}
  DolSection() {}
  uint32_t _file_off = 0, _vaddr = 0, _size = 0;
};

class DolData : public SectionedData {
private:
  reserved_vector<DolSection, 7> _text_sections;
  reserved_vector<DolSection, 11> _data_sections;
  DolSection _bss_section;

  bool sanity_check_header(size_t file_size);

public:
  static inline constexpr size_t kNumTextSections = 7;
  static inline constexpr size_t kNumDataSections = 11;

  bool load_from(std::istream& source);

  reserved_vector<DolSection, 7> const& text_section_headers() const { return _text_sections; }
  reserved_vector<DolSection, 11> const& data_section_headers() const { return _data_sections; }

  virtual ~DolData() {}
};

}  // namespace decomp
