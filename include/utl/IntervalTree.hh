#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decomp {

// Intervals have an inclusive lower bound and exclusive upper bound
template <typename T, typename IvType = uint32_t>
class dinterval_tree {
  static_assert(std::is_integral_v<IvType>, "Expected integral interval type");

  struct interval_node {
    IvType _lo, _hi, _st_max;
    T _val;
    interval_node* _lp = nullptr;
    interval_node* _rp = nullptr;
    interval_node* _parent;
    int64_t _height = 1;

    interval_node(IvType lo, IvType hi, T&& val, interval_node* parent)
        : _lo(lo), _hi(hi), _st_max(hi), _val(std::move(val)), _parent(parent) {}

    constexpr bool contains(IvType pt) const { return pt >= _lo && pt < _hi; }

    constexpr bool overlaps(IvType l, IvType r) const { return _hi > l && r > _lo; }

    constexpr int64_t balance_factor() const {
      return (_lp == nullptr ? 0 : _lp->_height) - (_rp == nullptr ? 0 : _rp->_height);
    }

    void fix_cached_values() {
      _height = 1;
      _st_max = _hi;
      if (_lp != nullptr) {
        _height = 1 + _lp->_height;
      }
      if (_rp != nullptr) {
        _height = std::max(_height, _rp->_height + 1);
        // Because of the discrete restriction, right pointer must have a max > left, this
        _st_max = _rp->_st_max;
      }
    }
  };

  static interval_node* l_rotate(interval_node* pivot) {
    interval_node* new_pivot = pivot->_rp;
    pivot->_rp = new_pivot->_lp;
    new_pivot->_lp = pivot;

    // fix parents
    if (pivot->_rp) {
      pivot->_rp->_parent = pivot;
    }
    pivot->_parent = new_pivot;

    // Update cache of pivoted node before new pivot
    pivot->fix_cached_values();
    new_pivot->fix_cached_values();

    return new_pivot;
  }

  static interval_node* r_rotate(interval_node* pivot) {
    interval_node* new_pivot = pivot->_lp;
    pivot->_lp = new_pivot->_rp;
    new_pivot->_rp = pivot;

    // fix parents
    if (pivot->_lp) {
      pivot->_lp->_parent = pivot;
    }
    pivot->_parent = new_pivot;

    // Update cache of pivoted node before new pivot
    pivot->fix_cached_values();
    new_pivot->fix_cached_values();

    return new_pivot;
  }

  static interval_node* lr_rotate(interval_node* base) {
    base->_lp = l_rotate(base->_lp);
    base->_lp->_parent = base;
    return r_rotate(base);
  }

  static interval_node* rl_rotate(interval_node* base) {
    base->_rp = r_rotate(base->_rp);
    base->_rp->_parent = base;
    return l_rotate(base);
  }

  interval_node* do_rotation(interval_node*(rot_f)(interval_node*), interval_node* base) {
    if (base == _root) {
      _root = rot_f(base);
      _root->_parent = nullptr;
      return nullptr;
    }

    interval_node* parent = base->_parent;
    if (parent->_lp == base) {
      parent->_lp = rot_f(base);
      parent->_lp->_parent = parent;
    } else {
      parent->_rp = rot_f(base);
      parent->_rp->_parent = parent;
    }

    parent->fix_cached_values();
    return parent;
  }

  // Incremental AVL rebalance
  void rebalance(interval_node* new_node) {
    interval_node* parent = new_node->_parent;
    while (parent != nullptr) {
      parent->_height = 1 + std::max(parent->_lp == nullptr ? 0 : parent->_lp->_height,
                                parent->_rp == nullptr ? 0 : parent->_rp->_height);

      int64_t bfac = parent->balance_factor();
      if (bfac > 1) {
        if (parent->_lp->balance_factor() > 0) {
          parent = do_rotation(r_rotate, parent);
        } else {
          parent = do_rotation(lr_rotate, parent);
        }
      } else if (bfac < -1) {
        if (parent->_rp->balance_factor() < 0) {
          parent = do_rotation(l_rotate, parent);
        } else {
          parent = do_rotation(rl_rotate, parent);
        }
      } else {
        parent = parent->_parent;
      }
    }
  }

  interval_node const* query_node(IvType low, IvType high) const {
    interval_node const* search = _root;
    while (search != nullptr) {
      if (search->overlaps(low, high)) {
        return search;
      } else if (search->_lp == nullptr) {
        search = search->_rp;
      } else if (search->_lp->_st_max <= low) {
        search = search->_rp;
      } else {
        search = search->_lp;
      }
    }

    return nullptr;
  }

  interval_node* query_node(IvType low, IvType high) {
    return const_cast<T*>(const_cast<dinterval_tree const*>(this)->query_node(low, high));
  }

  interval_node* _root = nullptr;

public:
  T const* query(IvType low, IvType high) const {
    interval_node const* node = query_node(low, high);
    if (node == nullptr) {
      return nullptr;
    }
    return &node->_val;
  }

  T* query(IvType low, IvType high) {
    return const_cast<T*>(const_cast<dinterval_tree const*>(this)->query(low, high));
  }

  template <typename... Args>
  bool cut_emplace_below(IvType cut_location, Args&&... args) {
    interval_node* split_node = query_node(cut_location, cut_location);
    if (split_node == nullptr) {
      return false;
    }

    IvType old_hi = split_node->_hi;
    split_node->_hi = cut_location;

    // Changing the interval forces a recache up the tree
    for (interval_node* n = split_node; n != nullptr; n = n->_parent) {
      n->fix_cached_values();
    }

    return try_emplace(cut_location, old_hi, std::forward<Args>(args)...);
  }

  template <typename... Args>
  bool try_emplace(IvType low, IvType high, Args&&... args) {
    // Disallow overlap
    if (query(low, high) != nullptr) {
      return false;
    }

    interval_node* parent = nullptr;
    interval_node** insert_ptr;
    if (_root == nullptr) {
      insert_ptr = &_root;
    } else {
      interval_node* search_ins = _root;
      while (search_ins != nullptr) {
        // Update maximum going downwards
        search_ins->_st_max = std::max(high, search_ins->_st_max);

        if (low < search_ins->_lo) {
          insert_ptr = &search_ins->_lp;
        } else {
          insert_ptr = &search_ins->_rp;
        }

        parent = search_ins;
        search_ins = *insert_ptr;
      }
    }

    *insert_ptr = new interval_node(low, high, T(std::forward<Args>(args)...), parent);

    rebalance(*insert_ptr);
    return true;
  }

  ~dinterval_tree() {
    if (_root == nullptr) {
      return;
    }

    std::vector<interval_node*> free_stack;
    free_stack.push_back(_root);
    while (!free_stack.empty()) {
      interval_node* next = free_stack.back();
      free_stack.pop_back();

      if (next->_lp != nullptr) {
        free_stack.push_back(next->_lp);
      }
      if (next->_rp != nullptr) {
        free_stack.push_back(next->_rp);
      }

      delete next;
    }
  }
};

}  // namespace decomp
