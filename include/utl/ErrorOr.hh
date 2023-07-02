#pragma once

#include <string>
#include <variant>

template <typename T>
struct ErrorOr {
  ErrorOr(std::string const& reason) : _val(reason) {}
  ErrorOr(std::string&& reason) : _val(std::move(reason)) {}
  ErrorOr(char const* reason) : _val(std::string(reason)) {}
  ErrorOr(T&& t) : _val(std::forward<T>(t)) {}

  std::variant<std::string, T> _val;

  constexpr bool is_error() const { return std::holds_alternative<std::string>(_val); }
  std::string const& err() const { return std::get<std::string>(_val); }
  T& val() { return std::get<T>(_val); }
  T const& val() const { return std::get<T>(_val); }
};
