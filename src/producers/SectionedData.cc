#include "producers/SectionedData.hh"

#include <algorithm>

namespace decomp {
namespace {
template <typename T>
std::optional<T> read_generic(dinterval_tree<Section> const& regions, uint32_t address) {
  Section const* sect = regions.query(address, address + sizeof(T));
  if (sect == nullptr) {
    return std::nullopt;
  }
  if (!sect->contains(address) || !sect->contains(address + sizeof(T) - 1)) {
    return std::nullopt;
  }

  T ret;
  uint8_t* t_write = reinterpret_cast<uint8_t*>(&ret) + sizeof(T);

  for (size_t i = 0; i < sizeof(T); i++) {
    *--t_write = sect->_data[i + (address - sect->_base)];
  }

  return ret;
}
}  // namespace

bool SectionedData::add_section(uint32_t base, std::string_view data) {
  return _regions.try_emplace(base, base + data.length(), base, std::vector<uint8_t>(data.begin(), data.end()));
}

bool SectionedData::add_section(uint32_t base, std::vector<uint8_t>&& data) {
  return _regions.try_emplace(base, base + data.size(), base, std::move(data));
}

bool SectionedData::add_section(uint32_t base, std::vector<uint8_t> const& data) {
  return _regions.try_emplace(base, base + data.size(), base, std::vector<uint8_t>(data));
}

uint8_t SectionedData::read_byte(uint32_t vaddr) const {
  auto opt = read_generic<uint8_t>(_regions, vaddr);
  if (!opt) {
    return 0;
  }
  return *opt;
}

uint16_t SectionedData::read_half(uint32_t vaddr) const {
  auto opt = read_generic<uint16_t>(_regions, vaddr);
  if (!opt) {
    return 0;
  }
  return *opt;
}

uint32_t SectionedData::read_word(uint32_t vaddr) const {
  auto opt = read_generic<uint32_t>(_regions, vaddr);
  if (!opt) {
    return 0;
  }
  return *opt;
}

uint64_t SectionedData::read_long(uint32_t vaddr) const {
  auto opt = read_generic<uint64_t>(_regions, vaddr);
  if (!opt) {
    return 0;
  }
  return *opt;
}

float SectionedData::read_float(uint32_t vaddr) const {
  auto opt = read_generic<float>(_regions, vaddr);
  if (!opt) {
    return 0.f;
  }
  return *opt;
}

double SectionedData::read_double(uint32_t vaddr) const {
  auto opt = read_generic<double>(_regions, vaddr);
  if (!opt) {
    return 0.;
  }
  return *opt;
}

Section const* SectionedData::section_for_vaddr(uint32_t vaddr) const { return _regions.query(vaddr, vaddr + 1); }

}  // namespace decomp
