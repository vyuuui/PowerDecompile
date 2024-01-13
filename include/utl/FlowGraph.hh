#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace decomp {
template <typename R, typename F, typename... Args>
concept invocable_r = std::invocable<F, Args...> && std::is_same_v<R, std::invoke_result_t<F, Args...>>;

template <typename T, typename U>
concept explicitly_convertible = requires { static_cast<U>(std::declval<T>()); };

class FlowGraphBase;

enum class BlockTransfer : uint32_t {
  kUnconditional,
  kConditionTrue,
  kConditionFalse,
  kFallthrough,

  kFirstSwitchCase = 0x4,
  kLastSwitchCase = 0xffffffff,
};

constexpr bool inverse_condition(BlockTransfer bt0, BlockTransfer bt1) {
  return (bt0 == BlockTransfer::kConditionTrue && bt1 == BlockTransfer::kConditionFalse) ||
         (bt0 == BlockTransfer::kConditionFalse && bt1 == BlockTransfer::kConditionTrue);
}

struct EdgeData {
  EdgeData() : _target(-1), _tr(BlockTransfer::kUnconditional) {}
  EdgeData(int target, BlockTransfer tr) : _target(target), _tr(tr) {}
  int _target;
  BlockTransfer _tr;

  constexpr bool operator==(EdgeData const& rhs) const { return _target == rhs._target && _tr == rhs._tr; }
  constexpr bool operator!=(EdgeData const& rhs) const { return !(*this == rhs); }
};

struct FlowVertexBase {
  FlowVertexBase(int idx) : _idx(idx) {}
  virtual ~FlowVertexBase() {}

  int _idx = -1;
  std::vector<EdgeData> _out;
  std::vector<EdgeData> _in;

  constexpr bool single_succ() const { return _out.size() == 1; }
  constexpr bool single_pred() const { return _in.size() == 1; }
};

class FlowGraphBase {
private:
  std::vector<std::unique_ptr<FlowVertexBase>> _vtx;
  int _root_id;
  int _terminal_id;

protected:
  // Disallow creation of untemplated FlowGraphBase type externally
  FlowGraphBase() : _root_id(kInvalidVertexId), _terminal_id(kInvalidVertexId) {}
  void add_existing_vertex(FlowVertexBase* v) { _vtx.emplace_back(v); }
  void set_root(int id) { _root_id = id; }
  void set_terminal(int id) { _terminal_id = id; }

public:
  inline static constexpr int kInvalidVertexId = -1;

  size_t size() const { return _vtx.size(); }

  FlowVertexBase* root() { return _vtx[_root_id].get(); }
  FlowVertexBase const* root() const { return _vtx[_root_id].get(); }

  FlowVertexBase* terminal() { return _vtx[_terminal_id].get(); }
  FlowVertexBase const* terminal() const { return _vtx[_terminal_id].get(); }

  FlowVertexBase* vertex(int idx) { return _vtx[idx].get(); }
  FlowVertexBase const* vertex(int idx) const { return _vtx[idx].get(); }
};

enum class PseudoVertexType { kUninitialized, kGraphPreheader, kGraphPostExit };

template <typename VertexData>
struct FlowVertex : public FlowVertexBase {
  FlowVertex(int idx)
      : FlowVertexBase(idx), _d(std::in_place_type<PseudoVertexType>, PseudoVertexType::kUninitialized) {}
  FlowVertex(int idx, PseudoVertexType vtype) : FlowVertexBase(idx), _d(std::in_place_type<PseudoVertexType>, vtype) {}
  virtual ~FlowVertex() {}
  VertexData& data() { return std::get<VertexData>(_d); }
  VertexData const& data() const { return std::get<VertexData>(_d); }
  PseudoVertexType pseudo() const { return std::get<PseudoVertexType>(_d); }

  std::variant<PseudoVertexType, VertexData> _d;

  constexpr bool is_real() const { return std::holds_alternative<VertexData>(_d); }
  constexpr bool is_pseudo() const { return std::holds_alternative<PseudoVertexType>(_d); }
  constexpr bool is_preheader() const {
    return std::holds_alternative<PseudoVertexType>(_d) &&
           std::get<PseudoVertexType>(_d) == PseudoVertexType::kGraphPreheader;
  }
  constexpr bool is_postexit() const {
    return std::holds_alternative<PseudoVertexType>(_d) &&
           std::get<PseudoVertexType>(_d) == PseudoVertexType::kGraphPostExit;
  }
};

template <typename VertexData>
  requires std::default_initializable<VertexData>
class FlowGraph : public FlowGraphBase {
public:
  using Vertex = FlowVertex<VertexData>;

  class iterator {
    FlowGraph* _owner;
    int _idx;

  public:
    using difference_type = int;
    using value_type = Vertex;
    using pointer = Vertex*;
    using reference = Vertex&;
    using iterator_category = std::input_iterator_tag;

    iterator(FlowGraph* owner, int idx) : _owner(owner), _idx(idx) {}
    reference operator*() const { return *_owner->vertex(_idx); }
    pointer operator->() const { return _owner->vertex(_idx); }
    iterator& operator++() {
      _idx++;
      return *this;
    }
    iterator operator++(int) {
      _idx++;
      return iterator(_owner, _idx - 1);
    }
    constexpr bool operator==(iterator const& rhs) const { return _owner == rhs._owner && _idx == rhs._idx; }
    constexpr bool operator!=(iterator const& rhs) const { return !(*this == rhs); }
  };

  class const_iterator {
    FlowGraph const* _owner;
    int _idx;

  public:
    using difference_type = int;
    using value_type = Vertex;
    using pointer = Vertex const*;
    using reference = Vertex const&;
    using iterator_category = std::input_iterator_tag;

    const_iterator(FlowGraph const* owner, int idx) : _owner(owner), _idx(idx) {}
    reference operator*() const { return *_owner->vertex(_idx); }
    pointer const* operator->() const { return _owner->vertex(_idx); }
    const_iterator& operator++() {
      _idx++;
      return *this;
    }
    const_iterator operator++(int) {
      _idx++;
      return const_iterator(_owner, _idx - 1);
    }
    constexpr bool operator==(const_iterator const& rhs) const { return _owner == rhs._owner && _idx == rhs._idx; }
    constexpr bool operator!=(const_iterator const& rhs) const { return !(*this == rhs); }
  };

  FlowGraph() {
    set_root(emplace_pseudovertex(PseudoVertexType::kGraphPreheader));
    set_terminal(emplace_pseudovertex(PseudoVertexType::kGraphPostExit));
  }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }

  Vertex* root() { return static_cast<Vertex*>(FlowGraphBase::root()); }
  Vertex const* root() const { return static_cast<Vertex const*>(FlowGraphBase::root()); }

  Vertex* terminal() { return static_cast<Vertex*>(FlowGraphBase::terminal()); }
  Vertex const* terminal() const { return static_cast<Vertex const*>(FlowGraphBase::terminal()); }

  Vertex* vertex(int idx) { return static_cast<Vertex*>(FlowGraphBase::vertex(idx)); }
  Vertex const* vertex(int idx) const { return static_cast<Vertex const*>(FlowGraphBase::vertex(idx)); }

  Vertex const* entrypoint() const { return root()->_out.size() > 0 ? vertex(root()->_out[0]._target) : nullptr; }
  Vertex* entrypoint() { return root()->_out.size() > 0 ? vertex(root()->_out[0]._target) : nullptr; }

  template <bool Forward, typename Visitor>
    requires std::invocable<Visitor, Vertex const&>
  void foreach_real_edge(Visitor&& visitor, Vertex const* v) const {
    for (auto [target, _] : (Forward ? v->_out : v->_in)) {
      if (vertex(target)->is_real()) {
        visitor(*vertex(target));
      }
    }
  }
  template <bool Forward, typename Visitor>
    requires std::invocable<Visitor, Vertex&>
  void foreach_real_edge(Visitor&& visitor, Vertex* v) {
    for (auto [target, _] : (Forward ? v->_out : v->_in)) {
      if (vertex(target)->is_real()) {
        visitor(*vertex(target));
      }
    }
  }

  template <typename Visitor>
    requires std::invocable<Visitor, Vertex const&>
  void foreach_real_outedge(Visitor&& visitor, Vertex const* v) const {
    foreach_real_edge<true>(std::forward<Visitor>(visitor), v);
  }
  template <typename Visitor>
    requires std::invocable<Visitor, Vertex&>
  void foreach_real_outedge(Visitor&& visitor, Vertex* v) {
    foreach_real_edge<true>(std::forward<Visitor>(visitor), v);
  }

  template <typename Visitor>
    requires std::invocable<Visitor, Vertex const&>
  void foreach_real_inedge(Visitor&& visitor, Vertex const* v) const {
    foreach_real_edge<false>(std::forward<Visitor>(visitor), v);
  }
  template <typename Visitor>
    requires std::invocable<Visitor, Vertex&>
  void foreach_real_inedge(Visitor&& visitor, Vertex* v) {
    foreach_real_edge<false>(std::forward<Visitor>(visitor), v);
  }

  template <typename Visitor>
    requires std::invocable<Visitor, Vertex const&>
  void foreach_exit(Visitor&& visitor) const {
    foreach_real_inedge(std::forward<Visitor>(visitor), terminal());
  }
  template <typename Visitor>
    requires std::invocable<Visitor, Vertex&>
  void foreach_exit(Visitor&& visitor) {
    foreach_real_inedge(std::forward<Visitor>(visitor), terminal());
  }

  bool is_exit_vertex(Vertex const* v) const {
    if (!v->is_real()) {
      return false;
    }
    if (v->_out.size() != 1) {
      return false;
    }
    return vertex(v->_out[0]._target)->is_postexit();
  }
  bool is_exit_vertex(int idx) const { return exit_vertex(vertex(idx)); }

  template <typename... VDArgs>
    requires std::constructible_from<VertexData, VDArgs...>
  int emplace_vertex(VDArgs&&... args) {
    Vertex* newv = new Vertex(size());
    newv->_d.template emplace<VertexData>(std::forward<VDArgs>(args)...);
    add_existing_vertex(newv);
    return newv->_idx;
  }

  int emplace_pseudovertex(PseudoVertexType pt) {
    Vertex* newv = new Vertex(size(), pt);
    add_existing_vertex(newv);
    return newv->_idx;
  }

  void emplace_link(Vertex* from, Vertex* to, BlockTransfer tr) {
    from->_out.emplace_back(to->_idx, tr);
    to->_in.emplace_back(from->_idx, tr);
  }

  void emplace_link(int from_idx, int to_idx, BlockTransfer tr) { emplace_link(vertex(from_idx), vertex(to_idx), tr); }

  // Insert a new node between `before` and all of its outgoing links
  int insert_after(Vertex* before, VertexData&& vdata, BlockTransfer tr) {
    const int new_idx = emplace_vertex(std::move(vdata));
    Vertex* newv = vertex(new_idx);
    newv->_out = std::move(before->_out);

    // Fixup input links
    for (auto [target, ttr] : newv->_out) {
      Vertex* fixupv = vertex(target);
      auto edge = std::find(fixupv->_in.begin(), fixupv->_in.end(), EdgeData(before->_idx, ttr));
      assert(edge != fixupv->_in.end());
      edge->_target = newv->_idx;
    }

    emplace_link(before, newv, tr);
    return new_idx;
  }

  // Insert a new node between `before_idx` and all of its outgoing links
  int insert_after(int before_idx, VertexData const& vdata, BlockTransfer tr) {
    return insert_after(vertex(before_idx), vdata, tr);
  }

  template <typename Visitor>
    requires std::invocable<Visitor, Vertex const&>
  void foreach_real(Visitor&& visitor) const {
    for (size_t i = 0; i < size(); i++) {
      if (vertex(i)->is_real()) {
        visitor(*vertex(i));
      }
    }
  }
  template <typename Visitor>
    requires std::invocable<Visitor, Vertex&>
  void foreach_real(Visitor&& visitor) {
    for (size_t i = 0; i < size(); i++) {
      if (vertex(i)->is_real()) {
        visitor(*vertex(i));
      }
    }
  }

  template <typename R, typename Visitor>
    requires invocable_r<R, Visitor, Vertex const&, R>
  R accumulate_real(Visitor&& visitor, R acc) const {
    for (size_t i = 0; i < size(); i++) {
      if (vertex(i)->is_real()) {
        acc = visitor(*vertex(i), acc);
      }
    }
    return acc;
  }
  template <typename R, typename Visitor>
    requires invocable_r<R, Visitor, Vertex&, R>
  R accumulate_real(Visitor&& visitor, R acc) {
    for (size_t i = 0; i < size(); i++) {
      if (vertex(i)->is_real()) {
        acc = visitor(*vertex(i), acc);
      }
    }
    return acc;
  }

  template <typename Visitor>
    requires std::predicate<Visitor, Vertex const&>
  Vertex* find_real(Visitor&& visitor) const {
    for (size_t i = 0; i < size(); i++) {
      if (vertex(i)->is_real()) {
        if (visitor(*vertex(i))) {
          return vertex(i);
        }
      }
    }
    return nullptr;
  }
  template <typename Visitor>
    requires std::predicate<Visitor, Vertex&>
  Vertex* find_real(Visitor&& visitor) {
    for (size_t i = 0; i < size(); i++) {
      if (vertex(i)->is_real()) {
        if (visitor(*vertex(i))) {
          return vertex(i);
        }
      }
    }
    return nullptr;
  }

  template <bool Forward, typename Visitor>
    requires std::invocable<Visitor, Vertex const&>
  void preorder(Visitor&& visitor, Vertex const* start) const {
    std::vector<bool> visited(size());
    std::vector<Vertex const*> process_stack;

    process_stack.emplace_back(start);

    while (!process_stack.empty()) {
      Vertex const* vert = process_stack.back();
      process_stack.pop_back();

      if (visited[vert->_idx]) {
        continue;
      }
      visited[vert->_idx] = true;

      visitor(*vert);

      for (auto [target, _] : (Forward ? vert->_out : vert->_in)) {
        if (!visited[target]) {
          process_stack.emplace_back(vertex(target));
        }
      }
    }
  }
  template <bool Forward, typename Visitor>
    requires std::invocable<Visitor, Vertex&>
  void preorder(Visitor&& visitor, Vertex* start) {
    std::vector<bool> visited(size());
    std::vector<Vertex*> process_stack;

    process_stack.emplace_back(start);

    while (!process_stack.empty()) {
      Vertex* vert = process_stack.back();
      process_stack.pop_back();

      if (visited[vert->_idx]) {
        continue;
      }
      visited[vert->_idx] = true;

      visitor(*vert);

      for (auto [target, _] : (Forward ? vert->_out : vert->_in)) {
        if (!visited[target]) {
          process_stack.emplace_back(vertex(target));
        }
      }
    }
  }

  template <typename Visitor>
    requires std::invocable<Visitor, Vertex const&>
  void preorder_fwd(Visitor&& visitor, Vertex const* start) const {
    preorder<true>(std::forward<Visitor>(visitor), start);
  }
  template <typename Visitor>
    requires std::invocable<Visitor, Vertex&>
  void preorder_fwd(Visitor&& visitor, Vertex* start) {
    preorder<true>(std::forward<Visitor>(visitor), start);
  }

  template <bool Forward, typename R, typename Visitor>
    requires invocable_r<R, Visitor, Vertex const&, R>
  void preorder_pathacc(Visitor&& visitor, Vertex const* start, R init) const {
    std::vector<bool> visited(size());
    std::vector<std::pair<Vertex const*, R>> process_stack;

    process_stack.emplace_back(start, init);

    while (!process_stack.empty()) {
      auto [vert, acc] = process_stack.back();
      process_stack.pop_back();

      if (visited[vert->_idx]) {
        continue;
      }
      visited[vert->_idx] = true;

      R feedforward = visitor(*vert, acc);

      for (auto [target, _] : (Forward ? vert->_out : vert->_in)) {
        if (!visited[target]) {
          process_stack.emplace_back(vertex(target), feedforward);
        }
      }
    }
  }
  template <bool Forward, typename R, typename Visitor>
    requires invocable_r<R, Visitor, Vertex&, R>
  void preorder_pathacc(Visitor&& visitor, Vertex* start, R init) {
    std::vector<bool> visited(size());
    std::vector<std::pair<Vertex*, R>> process_stack;

    process_stack.emplace_back(start, init);

    while (!process_stack.empty()) {
      auto [vert, acc] = process_stack.back();
      process_stack.pop_back();

      if (visited[vert->_idx]) {
        continue;
      }
      visited[vert->_idx] = true;

      R feedforward = visitor(*vert, acc);

      for (auto [target, _] : (Forward ? vert->_out : vert->_in)) {
        if (!visited[target]) {
          process_stack.emplace_back(vertex(target), feedforward);
        }
      }
    }
  }

  template <bool Forward, typename Visitor>
    requires std::invocable<Visitor, Vertex const&>
  void postorder(Visitor&& visitor, Vertex* start) const {
    std::vector<bool> visited(size());
    std::vector<std::pair<Vertex const*, int>> process_stack;
    std::vector<Vertex const*> path;
    process_stack.emplace_back(start, 0);

    while (!process_stack.empty()) {
      auto [vert, depth] = process_stack.back();
      process_stack.pop_back();

      if (visited[vert->_idx]) {
        continue;
      }
      visited[vert->_idx] = true;

      const int it_len = path.size() - depth;
      for (int i = 0; i < it_len; i++) {
        visitor(**(path.rbegin() + i));
      }
      path.resize(depth);
      path.push_back(vert);

      for (auto [target, _] : (Forward ? vert->_out : vert->_in)) {
        if (!visited[target]) {
          process_stack.emplace_back(vertex(target), depth + 1);
        }
      }
    }

    for (auto it = path.rbegin(); it != path.rend(); it++) {
      visitor(**it);
    }
  }
  template <bool Forward, typename Visitor>
    requires std::invocable<Visitor, Vertex&>
  void postorder(Visitor&& visitor, Vertex* start) {
    std::vector<bool> visited(size());
    std::vector<std::pair<Vertex*, int>> process_stack;
    std::vector<Vertex*> path;
    process_stack.emplace_back(start, 0);

    while (!process_stack.empty()) {
      auto [vert, depth] = process_stack.back();
      process_stack.pop_back();

      if (visited[vert->_idx]) {
        continue;
      }
      visited[vert->_idx] = true;

      const int it_len = path.size() - depth;
      for (int i = 0; i < it_len; i++) {
        visitor(**(path.rbegin() + i));
      }
      path.resize(depth);
      path.push_back(vert);

      for (auto [target, _] : (Forward ? vert->_out : vert->_in)) {
        if (!visited[target]) {
          process_stack.emplace_back(vertex(target), depth + 1);
        }
      }
    }

    for (auto it = path.rbegin(); it != path.rend(); it++) {
      visitor(**it);
    }
  }

  template <typename Visitor>
    requires std::invocable<Visitor, Vertex const&>
  void postorder_fwd(Visitor&& visitor, Vertex const* start) const {
    postorder<true>(std::forward<Visitor>(visitor), start);
  }
  template <typename Visitor>
    requires std::invocable<Visitor, Vertex&>
  void postorder_fwd(Visitor&& visitor, Vertex* start) {
    postorder<true>(std::forward<Visitor>(visitor), start);
  }

  template <typename RhsVertexData>
  void copy_shape(FlowGraph<RhsVertexData> const* from) {
    for (FlowVertex<RhsVertexData> const& rhsv : *from) {
      Vertex* v;
      if (rhsv.is_real()) {
        v = vertex(emplace_vertex());
      } else {
        v = vertex(emplace_pseudovertex(rhsv.pseudo()));
      }
      v->_out = rhsv._out;
      v->_in = rhsv._out;
    }
  }

  template <typename RhsVertexData, typename Generator>
    requires std::invocable<Generator, FlowVertex<RhsVertexData> const&, Vertex&>
  void copy_shape_generator(FlowGraph<RhsVertexData> const* from, Generator&& generator) {
    for (FlowVertex<RhsVertexData> const& rhsv : *from) {
      Vertex* v = vertex(emplace_pseudovertex(PseudoVertexType::kUninitialized));
      v->_out = rhsv._out;
      v->_in = rhsv._out;
      generator(rhsv, *v);
    }
  }
};
}  // namespace decomp
