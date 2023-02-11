#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "DisasmWrite.hh"
#include "producers/DolData.hh"
#include "SubroutineGraph.hh"

namespace {
using namespace decomp;

int graph_breakdown(char**);

struct CommandVerb {
  std::string_view name;
  std::string_view expect;
  int nargs;
  int(*verb_cb)(char**);

  void print_usage(char const* progname) const {
    std::cout << fmt::format("Expected usage for {} subcommand\n  {} {} {}\n",
                             name, progname, name, expect);
  }
};

std::array<CommandVerb, 1> sVerbs = {
  CommandVerb { "graph", "<path to DOL> <subroutine address>", 2, graph_breakdown },
};

void print_usage(char const* progname) {
  std::cout << fmt::format("Expected usage\n  {} <subcommand> ...\nSupported subcommands: ",
                           progname);

  std::cout << sVerbs[0].name;
  for (size_t i = 1; i < sVerbs.size(); i++) {
    std::cout << fmt::format(", {}", sVerbs[i].name);
  }

  std::cout << "\n";
}

// Verb implementations

int graph_breakdown(char** cmd_args) {
  uint32_t analysis_start = strtoll(cmd_args[1], nullptr, 16);

  DolData dol_data;
  {
    std::ifstream file_in(cmd_args[0], std::ios::binary);
    if (!file_in.is_open()) {
      std::cout << fmt::format("Failed to open path {}\n", cmd_args[0]);
      return 1;
    }
    if (!dol_data.load_from(file_in)) {
      std::cout << fmt::format("Provided file {} is not a DOL\n", cmd_args[0]);
      return 1;
    }
  }

  SubroutineGraph graph = create_graph(dol_data, analysis_start);
  std::vector<BasicBlock*> next;
  std::set<BasicBlock*> visited;
  next.push_back(graph.root);
  while (!next.empty()) {
    BasicBlock* cur = next.back();
    next.pop_back();
    if (visited.count(cur) > 0) {
      continue;
    }

    visited.emplace(cur);
    std::cout << fmt::format("Block 0x{:08x} -- 0x{:08x}\n", cur->block_start, cur->block_end);

    for (BasicBlock* out : cur->outgoing_edges) {
      if (visited.count(out) == 0) {
        next.push_back(out);
      }
    }
  }

  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 0) {
    print_usage("decompile");
    return 1;
  }

  if (argc == 1) {
    print_usage(argv[0]);
    return 1;
  }

  for (CommandVerb& verb : sVerbs) {
    if (argv[1] == verb.name) {
      if (argc - 2 != verb.nargs) {
        verb.print_usage(argv[0]);
        return 1;
      } else {
        return verb.verb_cb(argv + 2);
      }
    }
  }

  std::cout << fmt::format("Unknown subcommand \"{}\"\n", argv[1]);
  print_usage(argv[0]);
  return 1;
}
