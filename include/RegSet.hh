#pragma once

#include <cstdint>

#include "DataSource.hh"

namespace decomp {
template <typename T>
struct RegSet {
  uint32_t _set;
  constexpr RegSet() : _set(0) {}
  constexpr RegSet(uint32_t s) : _set(s) {}

  constexpr bool in_set(T reg) const { return ((1 << static_cast<uint8_t>(reg)) & _set) != 0; }
  constexpr bool empty() const { return _set == 0; }
  constexpr bool operator==(RegSet rhs) const { return _set == rhs._set; }
  constexpr bool operator!=(RegSet rhs) const { return _set != rhs._set; }

  // Add register to set
  constexpr RegSet& operator+=(T reg) { _set |= 1 << static_cast<uint8_t>(reg); return *this; }
  // Set union
  constexpr RegSet& operator+=(RegSet s) { _set |= s._set; return *this; }

  // Remove register from set
  constexpr RegSet& operator-=(T reg) { _set &= ~(1 << static_cast<uint8_t>(reg)); return *this; }
  // Set difference
  constexpr RegSet& operator-=(RegSet s) { _set &= ~s._set; return *this; }

  // Set intersection
  constexpr RegSet& operator&=(RegSet s) { _set &= s._set; return *this; }
  // Set uniq
  constexpr RegSet& operator^=(RegSet rhs) { _set ^= rhs._set; return *this; }

  // Set union
  constexpr RegSet operator+(RegSet rhs) const { return _set | rhs._set; }
  // Set difference
  constexpr RegSet operator-(RegSet rhs) const { return _set & ~rhs._set; }
  // Set intersection
  constexpr RegSet operator&(RegSet rhs) const { return _set & rhs._set; }
  // Set uniq
  constexpr RegSet operator^(RegSet rhs) const { return _set ^ rhs._set; }
};

using GprSet = RegSet<GPR>;
using FprSet = RegSet<FPR>;

template <GPR... gprs>
constexpr GprSet gpr_mask_literal() {
  return GprSet{(0 | ... | (1 << static_cast<uint8_t>(gprs)))};
}
template <FPR... gprs>
constexpr FprSet fpr_mask_literal() {
  return FprSet{(0 | ... | (1 << static_cast<uint8_t>(gprs)))};
}
template <typename... Ts>
constexpr GprSet gpr_mask(Ts... args) {
  return GprSet{(0 | ... | static_cast<uint32_t>(1 << static_cast<uint8_t>(args)))};
}
template <typename... Ts>
constexpr FprSet fpr_mask(Ts... args) {
  return FprSet{(0 | ... | static_cast<uint32_t>(1 << static_cast<uint8_t>(args)))};
}
}  // namespace decomp
