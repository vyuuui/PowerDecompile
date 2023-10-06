#include "Commands.hh"

#include <fmt/format.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <set>

#include "BinaryContext.hh"
#include "RegisterBinding.hh"
#include "Subroutine.hh"
#include "dbgutil/Disassembler.hh"
#include "producers/DolData.hh"
#include "utl/LaunchCommand.hh"

namespace decomp {
int summarize_subroutine(CommandParamList const& cpl) {
  uint32_t analysis_start = cpl.param_v<uint32_t>(1);

  BinaryContext ctx;
  {
    std::string const& path = cpl.param_v<std::string>(0);
    std::ifstream file_in(path, std::ios::binary);
    if (!file_in.is_open()) {
      std::cerr << fmt::format("Failed to open path {}\n", path);
      return 1;
    }

    ErrorOr<BinaryContext> result = create_from_stream(file_in, BinaryType::kDOL);
    if (result.is_error()) {
      std::cerr << fmt::format("Failed to open path {}, reason: {}\n", path, result.err());
      return 1;
    }

    ctx = std::move(result.val());
  }
  Subroutine subroutine;

  subroutine._graph = create_graph(*ctx._ram, analysis_start);
  evaluate_bindings(subroutine._graph, ctx);
  run_stack_analysis(subroutine._graph, subroutine._stack);

  constexpr auto types_list = [](TypeSet ts) {
    reserved_vector<char const*, 5> types;
    if (check_flags(ts, TypeSet::kByte)) {
      types.push_back("Byte");
    }
    if (check_flags(ts, TypeSet::kHalfWord)) {
      types.push_back("HalfWord");
    }
    if (check_flags(ts, TypeSet::kWord)) {
      types.push_back("Word");
    }
    if (check_flags(ts, TypeSet::kSingle)) {
      types.push_back("Single");
    }
    if (check_flags(ts, TypeSet::kDouble)) {
      types.push_back("Double");
    }
    return types;
  };
  constexpr auto reftype_str = [](ReferenceType reftype) {
    switch (reftype) {
      case ReferenceType::kRead:
        return "Read";
      case ReferenceType::kWrite:
        return "Write";
      case ReferenceType::kAddress:
        return "Address";
      default:
        return "";
    }
  };
  std::cout << fmt::format("Stack information:\n  Stack size: 0x{:x}\n", subroutine._stack._stack_size);
  for (StackVariable const& sv : subroutine._stack._stack_vars) {
    auto sv_types = types_list(sv._types);
    std::cout << fmt::format("    Variable offset 0x{:x} of type(s) ", sv._offset);
    for (char const* type_str : sv_types) {
      std::cout << type_str << " ";
    }
    std::cout << "\n";
    for (StackReference const& sr : sv._refs) {
      std::cout << fmt::format("      Referenced at 0x{:x} as a {}\n", sr._location, reftype_str(sr._reftype));
    }
  }

  std::vector<BasicBlock*> next;
  std::set<BasicBlock*> visited;
  next.push_back(subroutine._graph._root);
  while (!next.empty()) {
    BasicBlock* cur = next.back();
    next.pop_back();
    if (visited.count(cur) > 0) {
      continue;
    }

    visited.emplace(cur);
    std::cout << fmt::format("Block 0x{:08x} -- 0x{:08x}\n", cur->_block_start, cur->_block_end);

    RegisterLifetimes* bbp = cur->_block_lifetimes;
    std::cout << "  Input regs: ";
    for (uint32_t i = 0; i < 32; i++) {
      if (bbp->_input.in_set(static_cast<GPR>(i))) {
        std::cout << fmt::format("r{} ", i);
      }
    }

    std::cout << "\n  Output regs: ";
    for (uint32_t i = 0; i < 32; i++) {
      if (bbp->_output.in_set(static_cast<GPR>(i))) {
        std::cout << fmt::format("r{} ", i);
      }
    }
    std::cout << "\n  Overwritten regs: ";
    for (uint32_t i = 0; i < 32; i++) {
      if (bbp->_overwrite.in_set(static_cast<GPR>(i))) {
        std::cout << fmt::format("r{} ", i);
      }
    }
    std::cout << "\n";

    for (auto& pair : cur->_outgoing_edges) {
      BasicBlock* out = std::get<1>(pair);
      if (visited.count(out) == 0) {
        next.push_back(out);
      }
    }
  }

  for (Loop const& loop : subroutine._graph._loops) {
    std::cout << fmt::format("Loop beginning at 0x{:08x} spanning blocks:\n", loop._start->_block_start);

    for (BasicBlock* block : loop._contents) {
      std::cout << fmt::format("  Block 0x{:08x} -- 0x{:08x}\n", block->_block_start, block->_block_end);
    }
  }
  return 0;
}

int dump_dotfile(CommandParamList const& cpl) {
  uint32_t analysis_start = cpl.param_v<uint32_t>(1);

  DolData dol_data;
  {
    std::string const& path = cpl.param_v<std::string>(0);
    std::ifstream file_in(path, std::ios::binary);
    if (!file_in.is_open()) {
      std::cerr << fmt::format("Failed to open path {}\n", path);
      return 1;
    }
    if (!dol_data.load_from(file_in)) {
      std::cerr << fmt::format("Provided file {} is not a DOL\n", path);
      return 1;
    }
  }

  SubroutineGraph graph = create_graph(dol_data, analysis_start);

  std::string const& dot_path = cpl.option_v<std::string>("out");
  std::ofstream dotfile_out(dot_path, std::ios::trunc);
  if (!dotfile_out.is_open()) {
    std::cerr << fmt::format("Failed to open/create path {} for writing\n", dot_path);
  }

  dotfile_out << fmt::format("digraph sub_{:08x} {{\n  graph [splines=ortho]\n  {{\n", analysis_start);
  for (BasicBlock* block : graph._nodes_by_id) {
    dotfile_out << fmt::format(
        "    n{} [fontname=\"Courier New\" shape=\"box\" label=\"loc_{:08x}\\l", block->_block_id, block->_block_start);
    uint32_t i = 0;
    for (auto& inst : block->_instructions) {
      dotfile_out << fmt::format("{:08x}  ", block->_block_start + 4 * i);
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

  for (BasicBlock* block : graph._nodes_by_id) {
    if (block->_outgoing_edges.empty()) {
      continue;
    }

    for (auto&& [edge_type, next] : block->_outgoing_edges) {
      dotfile_out << fmt::format(
          "  n{} -> n{} [color=\"{}\"]\n", block->_block_id, next->_block_id, color_for_type(edge_type));
    }
  }

  dotfile_out << "}\n";
  dotfile_out.close();

  return 0;
}

int print_sections(CommandParamList const& cpl) {
  DolData dol_data;
  {
    std::string const& path = cpl.param_v<std::string>(0);
    std::ifstream file_in(path, std::ios::binary);
    if (!file_in.is_open()) {
      std::cerr << fmt::format("Failed to open path {}\n", path);
      return 1;
    }
    if (!dol_data.load_from(file_in)) {
      std::cerr << fmt::format("Provided file {} is not a DOL\n", path);
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

int linear_dis(CommandParamList const& cpl) {
  uint32_t disassembly_start = cpl.param_v<uint32_t>(1);
  int disassembly_len = cpl.param_v<uint32_t>(2);

  BinaryContext ctx;
  {
    std::string const& path = cpl.param_v<std::string>(0);
    std::ifstream file_in(path, std::ios::binary);
    if (!file_in.is_open()) {
      std::cerr << fmt::format("Failed to open path {}\n", path);
      return 1;
    }

    ErrorOr<BinaryContext> result = create_from_stream(file_in, BinaryType::kDOL);
    if (result.is_error()) {
      std::cerr << fmt::format("Failed to open path {}, reason: {}\n", path, result.err());
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
}  // namespace decomp
