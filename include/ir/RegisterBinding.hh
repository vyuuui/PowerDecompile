#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <tuple>
#include <vector>

#include "ir/IrInst.hh"
#include "ppc/RegSet.hh"
#include "ppc/Subroutine.hh"
#include "ppc/SubroutineGraph.hh"
#include "utl/IntervalTree.hh"

namespace decomp::ir {
constexpr uint32_t kInvalidTmp = 0xffffffff;

template <typename RType>
struct BindInfo {
  uint32_t _num;
  std::vector<std::pair<uint32_t, uint32_t>> _rgns;
  RType _reg;
  IrType _type;
  bool _is_param;
  bool _is_ret;

  BindInfo(uint32_t num, RType reg, bool is_param, bool is_ret)
      : _num(num), _reg(reg), _is_param(is_param), _is_ret(is_ret) {}
};

template <typename RType>
class BindTracker {
private:
  struct BlockBindInfo {
    uint32_t _ltemp_num;
    std::pair<uint32_t, uint32_t> _rgn;
    RType _reg;
    bool _is_param;
    bool _is_ret;
    // For union-find
    uint32_t _parent;
    std::optional<uint32_t> _cached_rep;
    std::optional<uint32_t> _gtemp_num;

    BlockBindInfo(uint32_t num, uint32_t lo, uint32_t hi, RType reg, bool is_param, bool is_ret)
        : _ltemp_num(num),
          _rgn(lo, hi),
          _reg(reg),
          _is_param(is_param),
          _is_ret(is_ret),
          _parent(num),
          _cached_rep(std::nullopt),
          _gtemp_num(std::nullopt) {}
  };

private:
  // List of all temporaries scoped to individual basic blocks
  std::vector<BlockBindInfo> _block_temps;
  std::vector<std::vector<uint32_t>> _forwarding_list;
  // List of all temporaries within this routine
  std::vector<BindInfo<RType>> _temps;
  // Mapping of register->temporary
  std::array<dinterval_tree<uint32_t>, 32> _temp_ranges;

private:
  BlockBindInfo& representative(uint32_t tid) {
    uint32_t next = tid;
    do {
      tid = next;
      next = _block_temps[tid]._parent;
    } while (next != tid);
    return _block_temps[tid];
  }

  void cache_representative(uint32_t tid) {
    uint32_t rep = tid;
    uint32_t cur;
    do {
      cur = rep;
      rep = _block_temps[cur]._cached_rep.value_or(_block_temps[cur]._parent);
    } while (rep != _block_temps[rep]._parent);

    cur = tid;
    while (!_block_temps[cur]._cached_rep) {
      _block_temps[cur]._cached_rep = rep;
      cur = _block_temps[cur]._parent;
    }
  }

public:
  BindTracker(ppc::Subroutine const& routine) { _forwarding_list.resize(routine._graph->_nodes_by_id.size()); }

  uint32_t add_block_bind(RType reg, bool is_param, bool is_ret, uint32_t lo, uint32_t hi) {
    const uint32_t new_temp = static_cast<uint32_t>(_block_temps.size());
    _block_temps.emplace_back(new_temp, lo, hi, reg, is_param, is_ret);
    return new_temp;
  }

  void publish_out(ppc::BasicBlock const& block, uint32_t idx) { _forwarding_list[block._block_id].emplace_back(idx); }

  void publish_in(ppc::BasicBlock const& block, uint32_t idx) {
    for (auto [_, in_block] : block._incoming_edges) {
      _forwarding_list[in_block->_block_id].emplace_back(idx);
    }
  }

  void collect_block_scope_temps() {
    // Union-find temps on block boundaries to create disjoint sets of block-scoped temps
    for (auto& fwl : _forwarding_list) {
      if (fwl.empty()) {
        continue;
      }

      while (!fwl.empty()) {
        // Union all temps sharing a common register on this edge
        RType search_reg = _block_temps[fwl[0]]._reg;
        auto grp = std::partition(
          fwl.begin(), fwl.end(), [this, search_reg](uint32_t tmp) { return _block_temps[tmp]._reg != search_reg; });
        BlockBindInfo& rep_grp = representative(*grp);
        for (auto it = grp + 1; it != fwl.end(); it++) {
          BlockBindInfo& rep_it = representative(*it);
          rep_it._parent = rep_grp._ltemp_num;
          rep_grp._is_param |= rep_it._is_param;
          rep_grp._is_ret |= rep_it._is_ret;
        }
        fwl.erase(grp, fwl.end());
      }
    }

    // Collect disjoint sets into routine-scoped locals
    for (BlockBindInfo& bbi : _block_temps) {
      cache_representative(bbi._ltemp_num);
      BlockBindInfo& rep = _block_temps[*bbi._cached_rep];
      if (!rep._gtemp_num) {
        rep._gtemp_num = static_cast<uint32_t>(_temps.size());
        _temps.emplace_back(*rep._gtemp_num, rep._reg, rep._is_param, rep._is_ret);
      }
      BindInfo<RType>& gtemp = _temps[*rep._gtemp_num];
      gtemp._rgns.emplace_back(bbi._rgn);
      assert(_temp_ranges[static_cast<uint8_t>(gtemp._reg)].try_emplace(bbi._rgn.first, bbi._rgn.second, gtemp._num));
    }

    // Clean up temporary data
    _block_temps.clear();
    _forwarding_list.clear();
  }

  BindInfo<RType> const* query_temp(uint32_t va, RType reg) const {
    uint32_t const* val = _temp_ranges[static_cast<uint8_t>(reg)].query(va, va + 4);
    if (val != nullptr) {
      return &_temps[*val];
    }
    return nullptr;
  }

  BindInfo<RType> const* get_temp(uint32_t idx) const {
    if (idx < _temps.size()) {
      return &_temps[idx];
    }
    return nullptr;
  }

  BindInfo<RType>* get_temp(uint32_t idx) {
    return const_cast<BindInfo<RType>*>(const_cast<BindTracker const*>(this)->get_temp(idx));
  }

  constexpr size_t ntemps() const { return _temps.size(); }
};

using GPRBindInfo = BindInfo<ppc::GPR>;
using GPRBindTracker = BindTracker<ppc::GPR>;
using FPRBindInfo = BindInfo<ppc::FPR>;
using FPRBindTracker = BindTracker<ppc::FPR>;
using CRBindInfo = BindInfo<ppc::CRField>;
using CRBindTracker = BindTracker<ppc::CRField>;
}  // namespace decomp::ir
