#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <sstream>
#include <utility>
#include <variant>

#include "utl/FlowGraph.hh"

using namespace decomp;

namespace doctest {
template <typename T>
struct StringMaker<std::vector<T>> {
  static String convert(std::vector<T> const& in) {
    std::ostringstream oss;

    oss << "[";
    for (auto it = in.begin(); it != in.end(); ++it) {
      oss << *it << ", ";
    }
    oss << "]";

    return oss.str().c_str();
  }
};
}  // namespace doctest

std::pair<FlowGraph<int>, std::array<int, 4>> make_multiexit_flowgraph() {
  FlowGraph<int> ret;

  std::array<int, 4> vertices;
  vertices[0] = ret.emplace_vertex(0);
  vertices[1] = ret.emplace_vertex(1);
  vertices[2] = ret.emplace_vertex(2);
  vertices[3] = ret.emplace_vertex(3);

  ret.emplace_link(ret.root()->_idx, vertices[0], BlockTransfer::kFallthrough);
  ret.emplace_link(vertices[0], vertices[1], BlockTransfer::kFallthrough);
  ret.emplace_link(vertices[1], ret.terminal()->_idx, BlockTransfer::kFallthrough);
  ret.emplace_link(vertices[2], ret.terminal()->_idx, BlockTransfer::kFallthrough);
  ret.emplace_link(vertices[3], ret.terminal()->_idx, BlockTransfer::kFallthrough);

  return std::make_pair(std::move(ret), vertices);
}

// Basic flowgraph, contains backedges, nested loops, nested acyclic structures, fully reducible
std::pair<FlowGraph<int>, std::array<int, 15>> make_basic_flowgraph() {
  FlowGraph<int> ret;
  std::array<int, 15> vertices;
  for (int i = 0; i < 15; i++) {
    vertices[i] = ret.emplace_vertex(i);
  }

  ret.emplace_link(vertices[0], vertices[1], BlockTransfer::kFallthrough);
  ret.emplace_link(vertices[1], vertices[2], BlockTransfer::kConditionFalse);
  ret.emplace_link(vertices[1], vertices[14], BlockTransfer::kConditionTrue);
  ret.emplace_link(vertices[2], vertices[3], BlockTransfer::kConditionTrue);
  ret.emplace_link(vertices[2], vertices[4], BlockTransfer::kConditionFalse);
  ret.emplace_link(vertices[3], vertices[5], BlockTransfer::kConditionTrue);
  ret.emplace_link(vertices[3], vertices[6], BlockTransfer::kConditionFalse);
  ret.emplace_link(vertices[4], vertices[7], BlockTransfer::kConditionTrue);
  ret.emplace_link(vertices[4], vertices[8], BlockTransfer::kConditionFalse);
  ret.emplace_link(vertices[5], vertices[9], BlockTransfer::kFallthrough);
  ret.emplace_link(vertices[6], vertices[9], BlockTransfer::kUnconditional);
  ret.emplace_link(vertices[7], vertices[9], BlockTransfer::kUnconditional);
  ret.emplace_link(vertices[8], vertices[9], BlockTransfer::kUnconditional);
  ret.emplace_link(vertices[9], vertices[10], BlockTransfer::kConditionTrue);
  ret.emplace_link(vertices[9], vertices[11], BlockTransfer::kConditionFalse);
  ret.emplace_link(vertices[10], vertices[11], BlockTransfer::kFallthrough);
  ret.emplace_link(vertices[11], vertices[12], BlockTransfer::kConditionFalse);
  ret.emplace_link(vertices[11], vertices[13], BlockTransfer::kConditionTrue);
  ret.emplace_link(vertices[12], vertices[11], BlockTransfer::kUnconditional);
  ret.emplace_link(vertices[13], vertices[1], BlockTransfer::kUnconditional);

  ret.emplace_link(ret.root()->_idx, vertices.front(), BlockTransfer::kFallthrough);
  ret.emplace_link(vertices.back(), ret.terminal()->_idx, BlockTransfer::kFallthrough);

  return std::make_pair(std::move(ret), vertices);
}

TEST_CASE("Test node iteration helpers") {
  auto [gr, vertices] = make_basic_flowgraph();

  int niter = 0;
  gr.foreach_real([&niter](FlowVertex<int>& fv) {
    CHECK(fv.is_real());
    niter++;
  });
  CHECK((gr.size() - 2) == niter);

  niter = gr.accumulate_real(
    [](FlowVertex<int>& fv, int niter) {
      CHECK(fv.is_real());
      return niter + 1;
    },
    0);
  CHECK((gr.size() - 2) == niter);

  FlowVertex<int>* fv;
  fv = gr.find_real([](FlowVertex<int>& fv) {
    CHECK(fv.is_real());
    return fv.data() == 12;
  });
  REQUIRE(fv != nullptr);
  CHECK(fv->is_real());
  CHECK(fv->data() == 12);
  CHECK(fv->_idx == vertices[12]);
}

TEST_CASE("Test node edge iteration helpers") {
  auto [gr, vertices] = make_basic_flowgraph();

  // Check entrance and exit
  gr.foreach_exit([&gr, &vertices](FlowVertex<int>& fv) {
    CHECK(gr.is_exit_vertex(&fv));
    CHECK(fv._idx == vertices.back());
  });
  gr.foreach_real_outedge([&vertices](FlowVertex<int>& fv) { CHECK(fv._idx == vertices.front()); }, gr.root());

  // Ensure the graph is well-formed (entrypoint has no inedge, exit has no outedge)
  gr.foreach_real_outedge([](FlowVertex<int>&) { CHECK(false); }, gr.terminal());
  gr.foreach_real_inedge([](FlowVertex<int>&) { CHECK(false); }, gr.root());

  // Ensure "real" restriction works
  gr.foreach_real_inedge([](FlowVertex<int>&) { CHECK(false); }, gr.vertex(vertices.front()));
  gr.foreach_real_outedge([](FlowVertex<int>&) { CHECK(false); }, gr.vertex(vertices.back()));

  auto [gr2, vertices2] = make_multiexit_flowgraph();
  gr2.foreach_exit([&gr2, &vertices2](FlowVertex<int>& fv) {
    CHECK(gr2.is_exit_vertex(&fv));
    CHECK(fv.data() < vertices2.size());
    vertices2[fv.data()] = -1;
  });
  CHECK(vertices2[0] != -1);
  CHECK(vertices2[1] == -1);
  CHECK(vertices2[2] == -1);
  CHECK(vertices2[3] == -1);
}

TEST_CASE("Test graph traversals") {
  auto [gr, vertices] = make_basic_flowgraph();

  std::vector<int> preordering;
  gr.preorder<true>(
    [&preordering](FlowVertex<int>& fv) {
      if (fv.is_real()) {
        preordering.push_back(fv.data());
      }
    },
    gr.root());

  std::vector<int> canonical_preordering = {
    gr.vertex(vertices[0])->data(),
    gr.vertex(vertices[1])->data(),
    gr.vertex(vertices[14])->data(),
    gr.vertex(vertices[2])->data(),
    gr.vertex(vertices[4])->data(),
    gr.vertex(vertices[8])->data(),
    gr.vertex(vertices[9])->data(),
    gr.vertex(vertices[11])->data(),
    gr.vertex(vertices[13])->data(),
    gr.vertex(vertices[12])->data(),
    gr.vertex(vertices[10])->data(),
    gr.vertex(vertices[7])->data(),
    gr.vertex(vertices[3])->data(),
    gr.vertex(vertices[6])->data(),
    gr.vertex(vertices[5])->data(),
  };

  CHECK(preordering == canonical_preordering);

  std::vector<int> reverse_preordering;
  gr.preorder<false>(
    [&reverse_preordering](FlowVertex<int>& fv) {
      if (fv.is_real()) {
        reverse_preordering.push_back(fv.data());
      }
    },
    gr.terminal());

  std::vector<int> canonical_rev_preordering = {
    gr.vertex(vertices[14])->data(),
    gr.vertex(vertices[1])->data(),
    gr.vertex(vertices[13])->data(),
    gr.vertex(vertices[11])->data(),
    gr.vertex(vertices[12])->data(),
    gr.vertex(vertices[10])->data(),
    gr.vertex(vertices[9])->data(),
    gr.vertex(vertices[8])->data(),
    gr.vertex(vertices[4])->data(),
    gr.vertex(vertices[2])->data(),
    gr.vertex(vertices[7])->data(),
    gr.vertex(vertices[6])->data(),
    gr.vertex(vertices[3])->data(),
    gr.vertex(vertices[5])->data(),
    gr.vertex(vertices[0])->data(),
  };

  CHECK(reverse_preordering == canonical_rev_preordering);

  std::vector<std::vector<int>> before_sets = {
    // After 0
    {
      vertices[1],
      vertices[2],
      vertices[3],
      vertices[4],
      vertices[5],
      vertices[6],
      vertices[7],
      vertices[8],
      vertices[9],
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
      vertices[14],
    },
    // After 1
    {
      vertices[2],
      vertices[3],
      vertices[4],
      vertices[5],
      vertices[6],
      vertices[7],
      vertices[8],
      vertices[9],
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
      vertices[14],
    },
    // After 2
    {
      vertices[3],
      vertices[4],
      vertices[5],
      vertices[6],
      vertices[7],
      vertices[8],
      vertices[9],
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
    },
    // After 3
    {
      vertices[5],
      vertices[6],
      vertices[9],
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
    },
    // After 4
    {
      vertices[7],
      vertices[8],
      vertices[9],
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
    },
    // After 5
    {
      vertices[9],
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
    },
    // After 6
    {
      vertices[9],
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
    },
    // After 7
    {
      vertices[9],
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
    },
    // After 8
    {
      vertices[9],
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
    },
    // After 9
    {
      vertices[10],
      vertices[11],
      vertices[12],
      vertices[13],
    },
    // After 10
    {
      vertices[11],
      vertices[12],
      vertices[13],
    },
    // After 11
    {
      vertices[12],
      vertices[13],
    },
    // After 12
    {},
    // After 13
    {},
    // After 14
    {},
  };
  for (auto& list : before_sets) {
    std::sort(list.begin(), list.end());
  }

  std::vector<int> postordering;
  gr.postorder<true>(
    [&before_sets, &postordering](FlowVertex<int>& fv) {
      if (fv.is_real()) {
        std::vector<int> sorted_postordering = postordering;
        std::sort(sorted_postordering.begin(), sorted_postordering.end());
        // sorted_postordering should contain all nodes iterated up to this one, which should be a superset of
        // before_sets[node]
        CHECK(std::includes(sorted_postordering.begin(),
          sorted_postordering.end(),
          before_sets[fv.data()].begin(),
          before_sets[fv.data()].begin()));

        postordering.push_back(fv.data());
      }
    },
    gr.root());
}

TEST_CASE("Test dominator tree") {
  auto [gr, vertices] = make_basic_flowgraph();

  std::vector<int> dom = gr.compute_dom_tree();
  std::vector<int> pdom = gr.compute_pdom_tree();

  std::vector<int> expected_dom(gr.size());
  expected_dom[gr.root()->_idx] = gr.root()->_idx;
  expected_dom[gr.terminal()->_idx] = vertices[14];
  expected_dom[vertices[0]] = gr.root()->_idx;
  expected_dom[vertices[1]] = vertices[0];
  expected_dom[vertices[2]] = vertices[1];
  expected_dom[vertices[3]] = vertices[2];
  expected_dom[vertices[4]] = vertices[2];
  expected_dom[vertices[5]] = vertices[3];
  expected_dom[vertices[6]] = vertices[3];
  expected_dom[vertices[7]] = vertices[4];
  expected_dom[vertices[8]] = vertices[4];
  expected_dom[vertices[9]] = vertices[2];
  expected_dom[vertices[10]] = vertices[9];
  expected_dom[vertices[11]] = vertices[9];
  expected_dom[vertices[12]] = vertices[11];
  expected_dom[vertices[13]] = vertices[11];
  expected_dom[vertices[14]] = vertices[1];
  CHECK(expected_dom == dom);

  std::vector<int> expected_pdom(gr.size());
  expected_pdom[gr.root()->_idx] = vertices[0];
  expected_pdom[gr.terminal()->_idx] = gr.terminal()->_idx;
  expected_pdom[vertices[0]] = vertices[1];
  expected_pdom[vertices[1]] = vertices[14];
  expected_pdom[vertices[2]] = vertices[9];
  expected_pdom[vertices[3]] = vertices[9];
  expected_pdom[vertices[4]] = vertices[9];
  expected_pdom[vertices[5]] = vertices[9];
  expected_pdom[vertices[6]] = vertices[9];
  expected_pdom[vertices[7]] = vertices[9];
  expected_pdom[vertices[8]] = vertices[9];
  expected_pdom[vertices[9]] = vertices[11];
  expected_pdom[vertices[10]] = vertices[11];
  expected_pdom[vertices[11]] = vertices[13];
  expected_pdom[vertices[12]] = vertices[11];
  expected_pdom[vertices[13]] = vertices[1];
  expected_pdom[vertices[14]] = gr.terminal()->_idx;
  CHECK(expected_pdom == pdom);
}
