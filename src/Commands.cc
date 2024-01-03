#include "Commands.hh"

#include <fmt/format.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <set>

#include "dbgutil/Disassembler.hh"
#include "dbgutil/IrPrinter.hh"
#include "ir/GekkoTranslator.hh"
#include "hll/Function.hh"
#include "ppc/BinaryContext.hh"
#include "ppc/Perilogue.hh"
#include "ppc/RegisterLiveness.hh"
#include "ppc/Subroutine.hh"
#include "ppc/SubroutineStack.hh"
#include "producers/DolData.hh"
#include "utl/LaunchCommand.hh"
#include "utl/VariantOverloaded.hh"

namespace decomp {
namespace {
char const* color_for_type(BlockTransfer type) {
  switch (type) {
    case BlockTransfer::kUnconditional:
    case BlockTransfer::kFallthrough:
      return "blue";
    case BlockTransfer::kConditionTrue:
      return "green";
    case BlockTransfer::kConditionFalse:
      return "red";
    default:
      return "black";
  }
}
}

int test_cmd(CommandParamList const& cpl) {
  using namespace ppc;

  BinaryContext ctx;
  {
    char const* path = "test.elf";
    std::ifstream file_in(path, std::ios::binary);
    if (!file_in.is_open()) {
      std::cerr << fmt::format("Failed to open path {}\n", path);
      return 1;
    }

    ErrorOr<BinaryContext> result = create_from_stream(file_in, BinaryType::kELF);
    if (result.is_error()) {
      std::cerr << fmt::format("Failed to open path {}, reason: {}\n", path, result.err());
      return 1;
    }
    ctx = std::move(result.val());
  }
  Subroutine subroutine;
  run_graph_analysis(subroutine, ctx, 0x10000);

  run_liveness_analysis(subroutine, ctx);
  run_stack_analysis(subroutine);
  run_perilogue_analysis(subroutine, ctx);
  ir::IrRoutine irr = ir::translate_subroutine(subroutine);

  for (size_t i = 0; i < irr._gpr_binds.ntemps(); i++) {
    ir::BindInfo<GPR> const* bi = irr._gpr_binds.get_temp(i);
    std::cout << fmt::format("Bind t{} on gpr r{} over range(s):", bi->_num, static_cast<uint8_t>(bi->_reg));
    for (auto const& [lo, hi] : bi->_rgns) {
      std::cout << fmt::format("[{:x}-{:x}] ", lo, hi);
    }
    std::cout << "\n";
  }

  for (ir::IrBlockVertex const& block : irr._graph) {
    write_block(block, std::cout);
    std::cout << "\n";
  }
  return 0;
}

int summarize_subroutine(CommandParamList const& cpl) {
  using namespace ppc;
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

  run_graph_analysis(subroutine, ctx, analysis_start);
  run_liveness_analysis(subroutine, ctx);
  run_stack_analysis(subroutine);

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
  std::cout << fmt::format("Stack information:\n  Stack size: 0x{:x}\n", subroutine._stack->stack_size());
  for (StackVariable const& sv : subroutine._stack->var_list()) {
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

  subroutine._graph->foreach_real([](BasicBlockVertex& bbv) {
    ppc::BasicBlock const& block = bbv.data();
    std::cout << fmt::format("Block 0x{:08x} -- 0x{:08x}\n", block._block_start, block._block_end);

    GprLiveness* rlt = block._gpr_lifetimes.get();
    std::cout << "  Input regs: ";
    for (uint32_t i = 0; i < 32; i++) {
      if (rlt->_input.in_set(static_cast<GPR>(i))) {
        std::cout << fmt::format("r{} ", i);
      }
    }

    std::cout << "\n  Output regs: ";
    for (uint32_t i = 0; i < 32; i++) {
      if (rlt->_output.in_set(static_cast<GPR>(i))) {
        std::cout << fmt::format("r{} ", i);
      }
    }
    std::cout << "\n  Overwritten regs: ";
    for (uint32_t i = 0; i < 32; i++) {
      if (rlt->_overwrite.in_set(static_cast<GPR>(i))) {
        std::cout << fmt::format("r{} ", i);
      }
    }
    std::cout << "\n";
  });

  run_perilogue_analysis(subroutine, ctx);
  ir::IrRoutine irg = ir::translate_subroutine(subroutine);

  for (size_t i = 0; i < irg._gpr_binds.ntemps(); i++) {
    ir::BindInfo<GPR> const* bi = irg._gpr_binds.get_temp(i);
    std::cout << fmt::format("Bind t{} on gpr r{} over range(s):", bi->_num, static_cast<uint8_t>(bi->_reg));
    for (auto const& [lo, hi] : bi->_rgns) {
      std::cout << fmt::format("[{:x}-{:x}] ", lo, hi);
    }
    std::cout << "\n";
  }
  return 0;
}

int dump_dotfile(CommandParamList const& cpl) {
  using namespace ppc;
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
  run_graph_analysis(subroutine, ctx, analysis_start);

  std::string const& dot_path = cpl.option_v<std::string>("out");
  std::ofstream dotfile_out(dot_path, std::ios::trunc);
  if (!dotfile_out.is_open()) {
    std::cerr << fmt::format("Failed to open/create path {} for writing\n", dot_path);
  }

  dotfile_out << fmt::format("digraph sub_{:08x} {{\n  graph [splines=ortho]\n  {{\n", analysis_start);
  subroutine._graph->foreach_real([&dotfile_out](BasicBlockVertex& bbv) {
    BasicBlock const& block = bbv.data();
    dotfile_out << fmt::format(
      "    n{} [fontname=\"Courier New\" shape=\"box\" label=\"loc_{:08x}\\l", bbv._idx, block._block_start);
    uint32_t i = 0;
    for (auto& inst : block._instructions) {
      dotfile_out << fmt::format("{:08x}  ", block._block_start + 4 * i);
      write_inst_disassembly(inst, dotfile_out);
      dotfile_out << "\\l";
      i++;
    }
    dotfile_out << "\"]\n";
  });
  dotfile_out << "  }\n";

  subroutine._graph->foreach_real([&dotfile_out](BasicBlockVertex& bbv) {
    if (bbv._out.empty()) {
      return;
    }

    for (auto [target, rule] : bbv._out) {
      dotfile_out << fmt::format(
        "  n{} -> n{} [color=\"{}\"]\n", bbv._idx, target, color_for_type(static_cast<BlockTransfer>(rule)));
    }
  });

  dotfile_out << "}\n";
  dotfile_out.close();

  return 0;
}

int print_sections(CommandParamList const& cpl) {
  using namespace ppc;
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
  using namespace ppc;
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
