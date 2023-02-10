#pragma once

#include <vector>

#include "IntervalTree.hh"
#include "producers/RandomAccessData.hh"

namespace decomp {
struct Section {
  Section(uint32_t base, std::vector<uint8_t>&& data) : _base(base), _data(std::move(data)) {}

  uint32_t _base;
  std::vector<uint8_t> _data;

  constexpr uint32_t left() const { return _base; }
  uint32_t right() const { return _base + static_cast<uint32_t>(_data.size()); }

  constexpr bool contains(uint32_t address) const { return address >= left() && address < right(); }

  constexpr bool overlaps(Section const& region) const {
    return contains(region.left()) || contains(region.right()) || region.contains(left());
  }
};

class SectionedData : public RandomAccessData {
private:
  dinterval_tree<Section> _regions;
  
public:
  bool add_section(uint32_t base, std::vector<uint8_t>&& data);
  bool add_section(uint32_t base, std::vector<uint8_t> const& data);

  virtual ~SectionedData() {}

  uint8_t read_byte(uint32_t vaddr) const override;
  uint16_t read_half(uint32_t vaddr) const override;
  uint32_t read_word(uint32_t vaddr) const override;
  uint64_t read_long(uint32_t vaddr) const override;
  float read_float(uint32_t vaddr) const override;
  double read_double(uint32_t vaddr) const override;
};


}  // namespace decomp
