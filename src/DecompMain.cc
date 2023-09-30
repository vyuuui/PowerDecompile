#include <fmt/format.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

#include "BinaryContext.hh"
#include "CodeWarriorABIConfiguration.hh"
#include "PpcDisasm.hh"
#include "RegisterBinding.hh"
#include "SubroutineGraph.hh"
#include "dbgutil/Disassembler.hh"
#include "producers/DolData.hh"

namespace {
using namespace decomp;

int summarize_subroutine(char**);
int dump_dotfile(char**);
int print_sections(char**);
int linear_dis(char**);

struct CommandVerb {
  std::string_view name;
  std::string_view expect;
  std::string_view short_desc;
  int nargs;
  int (*verb_cb)(char**);

  void print_usage(char const* progname) const {
    std::cerr << fmt::format("Expected usage for {} command\n  {} {} {}\n", name, progname, name, expect);
  }
};

std::array<CommandVerb, 4> sVerbs = {
    CommandVerb{"summarize",
        "<dol-path> <subroutine-address>",
        "Print out a summary of the subroutine starting at a specified address",
        2,
        summarize_subroutine},
    CommandVerb{"graphviz",
        "<DOT-path> <dol-path> <subroutine-address>",
        "Create a DOT file for visualization with graphviz",
        3,
        dump_dotfile},
    CommandVerb{"sections", "<dol-path>", "Print out a list of all sections from the specified DOL", 1, print_sections},
    CommandVerb{"dis",
        "<dol-path> <start-address> <length>",
        "Prints a linear disassembly of <length> instructions starting at the specified address",
        3,
        linear_dis},
};

void print_usage(char const* progname) {
  std::cerr << fmt::format("Expected usage\n  {} <command> ...\nSupported commands:\n", progname);

  for (size_t i = 0; i < sVerbs.size(); i++) {
    std::cerr << fmt::format("  {}\t{}\n", sVerbs[i].name, sVerbs[i].short_desc);
  }
}

// Verb implementations

int summarize_subroutine(char** cmd_args) {
  uint32_t analysis_start = strtoll(cmd_args[1], nullptr, 16);

  BinaryContext ctx;
  {
    std::ifstream file_in(cmd_args[0], std::ios::binary);
    if (!file_in.is_open()) {
      std::cerr << fmt::format("Failed to open path {}\n", cmd_args[0]);
      return 1;
    }

    ErrorOr<BinaryContext> result = create_from_stream(file_in, BinaryType::kDOL);
    if (result.is_error()) {
      std::cerr << fmt::format("Failed to open path {}, reason: {}\n", cmd_args[0], result.err());
      return 1;
    }

    ctx = std::move(result.val());
  }

  SubroutineGraph graph = create_graph(*ctx._ram, analysis_start);
  evaluate_bindings(graph, ctx);

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

    RegisterLifetimes* bbp = static_cast<RegisterLifetimes*>(cur->extension_data);
    std::cout << "\tInput regs: ";
    for (uint32_t i = 0; i < 32; i++) {
      if (bbp->_input.in_set(static_cast<GPR>(i))) {
        std::cout << fmt::format("r{} ", i);
      }
    }

    std::cout << "\n\tOutput regs: ";
    for (uint32_t i = 0; i < 32; i++) {
      if (bbp->_output.in_set(static_cast<GPR>(i))) {
        std::cout << fmt::format("r{} ", i);
      }
    }
    std::cout << "\n\tOverwritten regs: ";
    for (uint32_t i = 0; i < 32; i++) {
      if (bbp->_overwritten.in_set(static_cast<GPR>(i))) {
        std::cout << fmt::format("r{} ", i);
      }
    }
    std::cout << "\n";

    for (auto& pair : cur->outgoing_edges) {
      BasicBlock* out = std::get<1>(pair);
      if (visited.count(out) == 0) {
        next.push_back(out);
      }
    }
  }

  for (Loop const& loop : graph.loops) {
    std::cout << fmt::format("Loop beginning at 0x{:08x} spanning blocks:\n", loop.loop_start->block_start);

    for (BasicBlock* block : loop.loop_contents) {
      std::cout << fmt::format("\tBlock 0x{:08x} -- 0x{:08x}\n", block->block_start, block->block_end);
    }
  }
  return 0;
}

int dump_dotfile(char** cmd_args) {
  uint32_t analysis_start = strtoll(cmd_args[2], nullptr, 16);

  DolData dol_data;
  {
    std::ifstream file_in(cmd_args[1], std::ios::binary);
    if (!file_in.is_open()) {
      std::cerr << fmt::format("Failed to open path {}\n", cmd_args[1]);
      return 1;
    }
    if (!dol_data.load_from(file_in)) {
      std::cerr << fmt::format("Provided file {} is not a DOL\n", cmd_args[1]);
      return 1;
    }
  }

  SubroutineGraph graph = create_graph(dol_data, analysis_start);

  std::ofstream dotfile_out(cmd_args[0], std::ios::trunc);
  if (!dotfile_out.is_open()) {
    std::cerr << fmt::format("Failed to open/create path {} for writing\n", cmd_args[0]);
  }

  dotfile_out << fmt::format("digraph sub_{:08x} {{\n  graph [splines=ortho]\n  {{\n", analysis_start);
  for (BasicBlock* block : graph.nodes_by_id) {
    dotfile_out << fmt::format("    n{} [fontname=\"Courier New\" shape=\"box\" label=\"loc_{:08x}\\l", block->block_id, block->block_start);
    uint32_t i = 0;
    for (auto& inst : block->instructions) {
      dotfile_out << fmt::format("{:08x}  ", block->block_start + 4 * i);
      write_inst_disassembly(inst, dotfile_out);
      dotfile_out << "\\l";
      i++;
    }
    dotfile_out << "\"]\n";
  }
  dotfile_out << "  }\n";

  constexpr auto color_for_type = [](OutgoingEdgeType type) -> char const* {
    switch (type) {
      case OutgoingEdgeType::kUnconditional:
      case OutgoingEdgeType::kFallthrough:
        return "blue";
      case OutgoingEdgeType::kConditionTrue:
        return "green";
      case OutgoingEdgeType::kConditionFalse:
        return "red";
      default:
        return "black";
    }
  };

  for (BasicBlock* block : graph.nodes_by_id) {
    if (block->outgoing_edges.empty()) {
      continue;
    }

    for (auto&& [edge_type, next] : block->outgoing_edges) {
      dotfile_out << fmt::format(
          "  n{} -> n{} [color=\"{}\"]\n", block->block_id, next->block_id, color_for_type(edge_type));
    }
  }

  dotfile_out << "}\n";
  dotfile_out.close();

  return 0;
}

int print_sections(char** cmd_args) {
  DolData dol_data;
  {
    std::ifstream file_in(cmd_args[0], std::ios::binary);
    if (!file_in.is_open()) {
      std::cerr << fmt::format("Failed to open path {}\n", cmd_args[0]);
      return 1;
    }
    if (!dol_data.load_from(file_in)) {
      std::cerr << fmt::format("Provided file {} is not a DOL\n", cmd_args[0]);
      return 1;
    }
  }

  std::cout << "SECTION NAME   VA BEGIN       VA END         FILE BEGIN     FILE END       SIZE\n";
  int section_number = 0;
  for (DolSection const& ts : dol_data.text_section_headers()) {
    std::cout << std::setw(15) << std::left << fmt::format(".text{}", section_number);
    std::cout << fmt::format("{:08x}       {:08x}       {:08x}       {:08x}       {:08x}\n",
        ts._vaddr,
        ts._vaddr + ts._size,
        ts._file_off,
        ts._file_off + ts._size,
        ts._size);
    section_number++;
  }

  section_number = 0;
  for (DolSection const& ds : dol_data.data_section_headers()) {
    std::cout << std::setw(15) << std::left << fmt::format(".data{}", section_number);
    std::cout << fmt::format("{:08x}       {:08x}       {:08x}       {:08x}       {:08x}\n",
        ds._vaddr,
        ds._vaddr + ds._size,
        ds._file_off,
        ds._file_off + ds._size,
        ds._size);
    section_number++;
  }
  return 0;
}

int linear_dis(char** cmd_args) {
  uint32_t disassembly_start = strtoll(cmd_args[1], nullptr, 16);
  int disassembly_len = strtoll(cmd_args[2], nullptr, 16);

  BinaryContext ctx;
  {
    std::ifstream file_in(cmd_args[0], std::ios::binary);
    if (!file_in.is_open()) {
      std::cerr << fmt::format("Failed to open path {}\n", cmd_args[0]);
      return 1;
    }

    ErrorOr<BinaryContext> result = create_from_stream(file_in, BinaryType::kDOL);
    if (result.is_error()) {
      std::cerr << fmt::format("Failed to open path {}, reason: {}\n", cmd_args[0], result.err());
      return 1;
    }

    ctx = std::move(result.val());
  }

  std::cout << "ADDRESS         INST WORD       DISASSEMBLY\n";
  for (int i = 0; i < disassembly_len; i++) {
    uint32_t address = disassembly_start + i * 4;
    MetaInst inst = ctx._ram->read_instruction(address);
    std::cout << fmt::format("{:08x}        {:08x}        ", address, inst._binst._bytes);
    write_inst_disassembly(inst, std::cout);
    std::cout << "\n";
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

  std::cerr << fmt::format("Unknown subcommand \"{}\"\n", argv[1]);
  print_usage(argv[0]);
  return 1;
}
