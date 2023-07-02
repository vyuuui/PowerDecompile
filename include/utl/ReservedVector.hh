#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace decomp {
template <typename T>
class _vector_base {
public:
  class const_iterator {
  protected:
    T const* _it;

  public:
    explicit const_iterator(T const* it) : _it(it) {}
    T const& operator*() const { return *_it; }
    T const* operator->() const { return _it; }
    const_iterator& operator++() {
      ++_it;
      return *this;
    }
    const_iterator& operator--() {
      --_it;
      return *this;
    }
    const_iterator operator++(int) {
      auto ret = *this;
      ++_it;
      return ret;
    }
    const_iterator operator--(int) {
      auto ret = *this;
      --_it;
      return ret;
    }
    bool operator!=(const_iterator const& other) const { return _it != other._it; }
    bool operator==(const_iterator const& other) const { return _it == other._it; }
    const_iterator operator+(ptrdiff_t i) const { return const_iterator(_it + i); }
    const_iterator operator-(ptrdiff_t i) const { return const_iterator(_it - i); }
    const_iterator& operator+=(ptrdiff_t i) {
      _it += i;
      return *this;
    }
    const_iterator& operator-=(ptrdiff_t i) {
      _it -= i;
      return *this;
    }
    ptrdiff_t operator-(const_iterator const& rhs) const { return _it - rhs._it; }
    bool operator>(const_iterator const& rhs) const { return _it > rhs._it; }
    bool operator<(const_iterator const& rhs) const { return _it < rhs._it; }
    bool operator>=(const_iterator const& rhs) const { return _it >= rhs._it; }
    bool operator<=(const_iterator const& rhs) const { return _it <= rhs._it; }
    T const& operator[](ptrdiff_t i) const { return _it[i]; }
  };

  class iterator : public const_iterator {
  public:
    explicit iterator(T* it) : const_iterator(it) {}
    T& operator*() const { return *const_cast<T*>(const_iterator::_it); }
    T* operator->() const { return const_cast<T*>(const_iterator::_it); }
    iterator& operator++() {
      ++const_iterator::_it;
      return *this;
    }
    iterator& operator--() {
      --const_iterator::_it;
      return *this;
    }
    iterator operator++(int) {
      auto ret = *this;
      ++const_iterator::_it;
      return ret;
    }
    iterator operator--(int) {
      auto ret = *this;
      --const_iterator::_it;
      return ret;
    }
    iterator operator+(ptrdiff_t i) const { return iterator(const_cast<T*>(const_iterator::_it) + i); }
    iterator operator-(ptrdiff_t i) const { return iterator(const_cast<T*>(const_iterator::_it) - i); }
    iterator& operator+=(ptrdiff_t i) {
      const_iterator::_it += i;
      return *this;
    }
    iterator& operator-=(ptrdiff_t i) {
      const_iterator::_it -= i;
      return *this;
    }
    ptrdiff_t operator-(iterator const& rhs) const { return const_iterator::_it - rhs.const_iterator::_it; }
    bool operator>(iterator const& rhs) const { return const_iterator::_it > rhs.const_iterator::_it; }
    bool operator<(iterator const& rhs) const { return const_iterator::_it < rhs.const_iterator::_it; }
    bool operator>=(iterator const& rhs) const { return const_iterator::_it >= rhs.const_iterator::_it; }
    bool operator<=(iterator const& rhs) const { return const_iterator::_it <= rhs.const_iterator::_it; }
    T& operator[](ptrdiff_t i) const { return const_cast<T*>(const_iterator::_it)[i]; }
  };

  constexpr iterator const_cast_iterator(const_iterator it) { return iterator(const_cast<T*>(it._it)); }
};

template <typename T, size_t N>
class reserved_vector : public _vector_base<T> {
private:
  using base = _vector_base<T>;

public:
  using iterator = typename base::iterator;
  using const_iterator = typename base::const_iterator;

private:
  // Utilities
  union alignas(T) storage_t {
    struct {
    } _dummy;
    T _value;
    storage_t() : _dummy() {}
    ~storage_t() {}
  };

  T& _get(ptrdiff_t idx) { return _storage[idx]._value; }
  T const& _get(ptrdiff_t idx) const { return _storage[idx]._value; }

private:
  // Data Members
  size_t _size;
  storage_t _storage[N];

  // Shortened with constexpr if
  template <typename Tp>
  static void destroy(Tp& v) {
    if constexpr (std::is_destructible_v<Tp> && !std::is_trivially_destructible_v<Tp>) {
      v.Tp::~Tp();
    }
  }

public:
  reserved_vector() : _size(0) {}

  void push_back(T const& v) {
    assert(_size < N);
    new (&_get(_size)) T(v);
    ++_size;
  }

  void push_back(T&& v) {
    assert(_size < N);
    new (&_get(_size)) T(std::forward<T>(v));
    ++_size;
  }

  template <typename... TArgs>
  T& emplace_back(TArgs... args) {
    assert(_size < N);
    T& ret = _get(_size);
    new (&_get(_size)) T(std::forward<TArgs>(args)...);

    ++_size;
    return ret;
  }

  void pop_back() {
    assert(_size > 0);
    --_size;
    destroy(_get(_size));
  }

  iterator insert(const_iterator pos, T const& val) {
    assert(_size < N);
    iterator target_pos = base::const_cast_iterator(pos);
    if (target_pos == end()) {
      push_back(val);
    } else {
      new (&_get(_size)) T(std::forward<T>(_get(_size - 1)));
      for (auto it = end() - 1; it != target_pos; --it) {
        *it = std::forward<T>(*(it - 1));
      }
      *target_pos = val;
      ++_size;
    }
    return target_pos;
  }

  iterator insert(const_iterator pos, T&& val) {
    assert(_size < N);
    iterator target_pos = base::const_cast_iterator(pos);
    if (target_pos == end()) {
      push_back(val);
    } else {
      new (_get(_size)) T(std::forward<T>(_get(_size - 1)));
      for (auto it = end() - 1; it != target_pos; --it) {
        *it = std::forward<T>(*(it - 1));
      }
      *target_pos = std::forward<T>(val);
      ++_size;
    }
    return target_pos;
  }

  void resize(size_t size) {
    assert(_size < N);
    if (size > _size) {
      for (size_t i = _size; i < size; ++i) {
        new (_get(i)) T();
      }
      _size = size;
    } else if (size < _size) {
      if constexpr (std::is_destructible_v<T> && !std::is_trivially_destructible_v<T>) {
        for (size_t i = size; i < _size; ++i) {
          destroy(_get(i));
        }
      }
      _size = size;
    }
  }

  iterator erase(const_iterator pos) {
    assert(_size > 0);
    for (auto it = base::const_cast_iterator(pos) + 1; it != end(); ++it) {
      *(it - 1) = std::forward<T>(*it);
    }
    --_size;
    destroy(_get(_size));
    return base::const_cast_iterator(pos);
  }

  void pop_front() {
    assert(_size > 0);
    if (_size != 0) {
      erase(begin());
    }
  }

  void clear() { resize(0); }

  constexpr size_t size() const { return _size; }
  constexpr bool empty() const { return _size == 0; }
  constexpr size_t capacity() const { return N; }
  T const* data() const { return std::addressof(_get(0)); }
  T* data() { return std::addressof(_get(0)); }

  T& back() { return _get(_size - 1); }
  T& front() { return _get(0); }
  T const& back() const { return _get(_size - 1); }
  T const& front() const { return _get(0); }

  iterator begin() { return iterator(std::addressof(_get(0))); }
  iterator end() { return iterator(std::addressof(_get(_size))); }
  const_iterator begin() const { return const_iterator(std::addressof(_get(0))); }
  const_iterator end() const { return const_iterator(std::addressof(_get(_size))); }

  T& operator[](size_t idx) { return _get(idx); }
  T const& operator[](size_t idx) const { return _get(idx); }

  ~reserved_vector() {
    if constexpr (std::is_destructible_v<T> && !std::is_trivially_destructible_v<T>) {
      for (size_t i = 0; i < _size; ++i) {
        destroy(_get(i));
      }
    }
  }
};
}  // namespace decomp
