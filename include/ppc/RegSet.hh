#pragma once

#include <cstdint>

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
}  // namespace decomp::ppc
