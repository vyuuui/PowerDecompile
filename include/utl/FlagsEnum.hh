#pragma once

#include <type_traits>

namespace decomp {
template <typename T>
constexpr T bit_or(T lhs, T rhs) {
  using PrimType = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<PrimType>(lhs) | static_cast<PrimType>(rhs));
}

template <typename T>
constexpr T bit_and(T lhs, T rhs) {
  using PrimType = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<PrimType>(lhs) & static_cast<PrimType>(rhs));
}

template <typename T>
constexpr T bit_xor(T lhs, T rhs) {
  using PrimType = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<PrimType>(lhs) ^ static_cast<PrimType>(rhs));
}

template <typename T>
constexpr T bit_not(T flags) {
  return flags ^ T::kAll;
}

template <typename T>
constexpr bool check_flags(T check, T against) {
  return (check & against & T::kAll) != T::kNone;
}

template <typename T>
constexpr bool any_flags(T flags) {
  return (flags & T::kAll) != T::kNone;
}
}  // namespace decomp

#define GEN_FLAG_OPERATORS(EnumType)                                                               \
  constexpr EnumType operator|(EnumType lhs, EnumType rhs) { return ::decomp::bit_or(lhs, rhs); }  \
  constexpr EnumType operator&(EnumType lhs, EnumType rhs) { return ::decomp::bit_and(lhs, rhs); } \
  constexpr EnumType operator^(EnumType lhs, EnumType rhs) { return ::decomp::bit_xor(lhs, rhs); } \
  constexpr EnumType operator~(EnumType flags) { return ::decomp::bit_not(flags); }
