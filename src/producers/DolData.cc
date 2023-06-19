#include "producers/DolData.hh"

#include <array>
#include <istream>

namespace decomp {
namespace {
template <typename T>
struct byteswap_prim {
  byteswap_prim(T& val) : _val(val) {}
  T& _val;
};

template <typename T>
std::istream& operator>>(std::istream& in, byteswap_prim<T> prim) {
  char* internal = reinterpret_cast<char*>(&prim._val);
  in.read(internal, sizeof(T));

  for (size_t i = 0; i < (sizeof(T) / 2); i++) {
    std::swap(internal[i], internal[sizeof(T) - i - 1]);
  }

  return in;
}
constexpr uint32_t kGcMinVa = 0x80000000;
constexpr uint32_t kGcMaxVa = 0x81800000;
constexpr uint32_t kRamSize = 0x1800000;
}  // namespace

bool DolData::sanity_check_header(size_t file_size) {
  size_t net_section_sizes = 0;

  for (DolSection const& tsect : _text_sections) {
    if (static_cast<size_t>(tsect._file_off) >= file_size) {
      return false;
    }
    if (tsect._vaddr < kGcMinVa || tsect._vaddr >= kGcMaxVa) {
      return false;
    }
    net_section_sizes += static_cast<size_t>(tsect._size);
  }

  for (DolSection const& dsect : _data_sections) {
    if (static_cast<size_t>(dsect._file_off) >= file_size) {
      return false;
    }
    if (dsect._vaddr < kGcMinVa || dsect._vaddr >= kGcMaxVa) {
      return false;
    }
    net_section_sizes += static_cast<size_t>(dsect._size);
  }

  if (_bss_section._vaddr < kGcMinVa || _bss_section._vaddr >= kGcMaxVa) {
    return false;
  }

  net_section_sizes += static_cast<size_t>(_bss_section._size);
  return net_section_sizes < kRamSize;
}

bool DolData::load_from(std::istream& source) {
  std::array<uint32_t, kNumTextSections> text_offs, text_vas, text_sizes;
  std::array<uint32_t, kNumDataSections> data_offs, data_vas, data_sizes;
  uint32_t bss_va, bss_size;
  for (size_t i = 0; i < kNumTextSections; i++) {
    source >> byteswap_prim(text_offs[i]);
  }
  for (size_t i = 0; i < kNumDataSections; i++) {
    source >> byteswap_prim(data_offs[i]);
  }
  for (size_t i = 0; i < kNumTextSections; i++) {
    source >> byteswap_prim(text_vas[i]);
  }
  for (size_t i = 0; i < kNumDataSections; i++) {
    source >> byteswap_prim(data_vas[i]);
  }
  for (size_t i = 0; i < kNumTextSections; i++) {
    source >> byteswap_prim(text_sizes[i]);
  }
  for (size_t i = 0; i < kNumDataSections; i++) {
    source >> byteswap_prim(data_sizes[i]);
  }
  source >> byteswap_prim(bss_va);
  source >> byteswap_prim(bss_size);

  for (size_t i = 0; i < kNumTextSections; i++) {
    if (text_offs[i] == 0 && text_vas[i] == 0 && text_sizes[i] == 0) {
      continue;
    }
    _text_sections.emplace_back(text_offs[i], text_vas[i], text_sizes[i]);
  }
  for (size_t i = 0; i < kNumDataSections; i++) {
    if (data_offs[i] == 0 && data_vas[i] == 0 && data_sizes[i] == 0) {
      continue;
    }
    _data_sections.emplace_back(data_offs[i], data_vas[i], data_sizes[i]);
  }
  _bss_section._vaddr = bss_va;
  _bss_section._size = bss_size;

  source.seekg(0, std::ios::end);
  size_t file_size = source.tellg();
  if (!sanity_check_header(file_size)) {
    return false;
  }

  for (DolSection const& tsect : _text_sections) {
    std::vector<uint8_t> raw_readout;
    raw_readout.resize(tsect._size);
    source.seekg(tsect._file_off, std::ios::beg);
    source.read(reinterpret_cast<char*>(raw_readout.data()), raw_readout.size());
    add_section(tsect._vaddr, std::move(raw_readout));
  }

  for (DolSection const& dsect : _data_sections) {
    std::vector<uint8_t> raw_readout;
    raw_readout.resize(dsect._size);
    source.seekg(dsect._file_off, std::ios::beg);
    source.read(reinterpret_cast<char*>(raw_readout.data()), raw_readout.size());
    add_section(dsect._vaddr, std::move(raw_readout));
  }

  return true;
}
}  // namespace decomp
