#pragma once

#include <string>
#include <variant>
#include <vector>

// Either monad
template <typename L, typename R>
  requires std::movable<L> && std::movable<R>
struct Either {
  using Rt = R;
  using Lt = R;
  Either(L const& left) : _val(left) {}
  Either(L&& left) : _val(std::move(left)) {}
  Either(R const& right) : _val(right) {}
  Either(R&& right) : _val(std::move(right)) {}

  constexpr bool is_left() const { return std::holds_alternative<L>(_val); }
  constexpr bool is_right() const { return std::holds_alternative<R>(_val); }

  L& left() { return std::get<L>(_val); }
  L const& left() const { return std::get<L>(_val); }
  R& right() { return std::get<R>(_val); }
  R const& right() const { return std::get<R>(_val); }

  constexpr operator bool() const { return is_right(); }

  // Bind operator, use >= since >>= has RtL associativity
  // (>=) :: Monad m => m a -> (a -> m b) -> m b
  template <typename F>
    requires std::invocable<F, R&&> &&
             std::is_same_v<Either<L, typename std::invoke_result_t<F, R&&>::Rt>, std::invoke_result_t<F, R&&>>
  auto operator>=(F&& f) -> std::invoke_result_t<F, R&&> {
    if (*this) {
      return f(std::move(std::get<R>(_val)));
    }
    return (typename std::invoke_result_t<F, R&&>)(std::move(left()));
  }

  std::variant<L, R> _val;
};

template <typename T>
struct ErrorOr : public Either<std::string, T> {
  ErrorOr(char const* err) : Either<std::string, T>(std::string(err)) {}
  ErrorOr(std::string const& err) : Either<std::string, T>(err) {}
  ErrorOr(std::string&& err) : Either<std::string, T>(std::move(err)) {}
  ErrorOr(T const& t) : Either<std::string, T>(t) {}
  ErrorOr(T&& t) : Either<std::string, T>(std::move(t)) {}

  constexpr bool is_error() const { return Either<std::string, T>::is_left(); }
  std::string const& err() const { return Either<std::string, T>::left(); }

  T const& val() const { return Either<std::string, T>::right(); }
  T& val() { return Either<std::string, T>::right(); }
};
