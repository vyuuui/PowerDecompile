#pragma once

#include <cstdint>

#include "ppc/DataSource.hh"

namespace decomp::ppc {
template <typename T>
struct RegSet {
  using RegType = T;

  uint32_t _set;
  constexpr RegSet() : _set(0) {}
  constexpr RegSet(uint32_t s) : _set(s) {}

  constexpr bool in_set(T reg) const { return ((1 << static_cast<uint8_t>(reg)) & _set) != 0; }
  constexpr bool empty() const { return _set == 0; }
  constexpr operator bool() const { return !empty(); }
  constexpr bool operator==(RegSet rhs) const { return _set == rhs._set; }
  constexpr bool operator!=(RegSet rhs) const { return _set != rhs._set; }

  constexpr RegSet operator+(T rhs) const { return _set + (1 << static_cast<uint8_t>(rhs)); }
  constexpr RegSet operator-(T rhs) const { return _set & ~(1 << static_cast<uint8_t>(rhs)); }
  constexpr RegSet operator&(T rhs) const { return _set & (1 << static_cast<uint8_t>(rhs)); }
  constexpr RegSet operator^(T rhs) const { return _set ^ (1 << static_cast<uint8_t>(rhs)); }

  // Add register to set
  constexpr RegSet& operator+=(T reg) {
    _set |= 1 << static_cast<uint8_t>(reg);
    return *this;
  }
  // Set union
  constexpr RegSet& operator+=(RegSet s) {
    _set |= s._set;
    return *this;
  }

  // Remove register from set
  constexpr RegSet& operator-=(T reg) {
    _set &= ~(1 << static_cast<uint8_t>(reg));
    return *this;
  }
  // Set difference
  constexpr RegSet& operator-=(RegSet s) {
    _set &= ~s._set;
    return *this;
  }

  // Set intersection
  constexpr RegSet& operator&=(RegSet s) {
    _set &= s._set;
    return *this;
  }
  // Set uniq
  constexpr RegSet& operator^=(RegSet rhs) {
    _set ^= rhs._set;
    return *this;
  }

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

constexpr GprSet kAllGprs = GprSet(0xffffffff);
constexpr FprSet kAllFprs = FprSet(0xffffffff);

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

constexpr GprSet gpr_range(GPR start) {
  GprSet excl = gpr_mask(start)._set - 1;
  return kAllGprs - excl;
}
constexpr FprSet fpr_range(FPR start) {
  FprSet excl = fpr_mask(start)._set - 1;
  return kAllFprs - excl;
}
}  // namespace decomp::ppc
