#pragma once

#include <vector>

#include "hll/Structurizer.hh"

namespace decomp::hll {
class AbstractControlNode;

struct Substitutor {
  AbstractControlNode* _repl;
  ACNGraph& _gr;

  Substitutor(AbstractControlNode* repl, ACNGraph& gr)
      : _repl(repl), _gr(gr) {}

  virtual void substitute() = 0;
  virtual std::vector<int> membership() = 0;
  virtual ~Substitutor() {}
};

struct SeqSubstitutor : public Substitutor {
  std::vector<ACNVertex*> _list;

  SeqSubstitutor(AbstractControlNode* repl,
    ACNGraph& gr,
    std::vector<ACNVertex*>&& list)
      : Substitutor(repl, gr), _list(list) {}

  void substitute() override;
  std::vector<int> membership() override;
  ~SeqSubstitutor() override {}
};

struct IfSubstitutor : public Substitutor {
  ACNVertex* _hdr;
  ACNVertex* _true;
  ACNVertex* _next;

  IfSubstitutor(AbstractControlNode* repl,
    ACNGraph& gr,
    ACNVertex* hdr,
    ACNVertex* t,
    ACNVertex* next)
      : Substitutor(repl, gr), _hdr(hdr), _true(t), _next(next) {}

  void substitute() override;
  std::vector<int> membership() override;
  ~IfSubstitutor() override {}
};

struct IfElseSubstitutor : public Substitutor {
  ACNVertex* _hdr;
  ACNVertex* _true;
  ACNVertex* _false;
  ACNVertex* _next;

  IfElseSubstitutor(AbstractControlNode* repl,
    ACNGraph& gr,
    ACNVertex* hdr,
    ACNVertex* t,
    ACNVertex* f,
    ACNVertex* next)
      : Substitutor(repl, gr), _hdr(hdr), _true(t), _false(f), _next(next) {}

  void substitute() override;
  std::vector<int> membership() override;
  ~IfElseSubstitutor() override {}
};

struct CCondSubstitutor : public Substitutor {
  std::vector<ACNVertex*> _nodes;
  CCondSubstitutor(AbstractControlNode* repl,
    ACNGraph& gr,
    std::vector<ACNVertex*>&& nodes)
      : Substitutor(repl, gr), _nodes(nodes) {}

  void substitute() override;
  std::vector<int> membership() override;
  ~CCondSubstitutor() override {}
};

struct SwitchSubstitutor : public Substitutor {};

}  // namespace decomp::hll
