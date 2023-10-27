#include "Commands.hh"

#include <fmt/format.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <set>

#include "dbgutil/Disassembler.hh"
#include "ir/GekkoTranslator.hh"
#include "ppc/BinaryContext.hh"
#include "ppc/Perilogue.hh"
#include "ppc/RegisterLiveness.hh"
#include "ppc/Subroutine.hh"
#include "producers/DolData.hh"
#include "utl/LaunchCommand.hh"
#include "utl/VariantOverloaded.hh"

namespace decomp {
int test_cmd(CommandParamList const& cpl) {
  using namespace ppc;

  BinaryContext ctx = create_raw(0,
    0,
    "\x94\x21\xFF\xF0\x93\xE1\x00\x0C\x3B\xE0\x00\x00\x2C\x03\x00\x03\x40\x82\x00\x0C\x3B\xE0\x00\x06\x48\x00\x00\x08"
    "\x38\x60\x00\x03\x7C\x63\xFA\x14\x83\xE1\x00\x0C\x38\x21\x00\x10\x4E\x80\x00\x20");

  Subroutine subroutine;
  subroutine._graph = create_graph(*ctx._ram, 0);

  run_liveness_analysis(subroutine._graph, ctx);
  subroutine._stack.run_stack_analysis(subroutine._graph);
  run_perilogue_analysis(subroutine, ctx);
  ir::IrGraph irg = ir::translate_subroutine(subroutine);

  for (size_t i = 0; i < irg._gpr_binds.ntemps(); i++) {
    ir::BindInfo<GPR> const* bi = irg._gpr_binds.get_temp(i);
    std::cout << fmt::format("Bind t{} on gpr r{} over range(s):", bi->_num, static_cast<uint8_t>(bi->_reg));
    for (auto const& [lo, hi] : bi->_rgns) {
      std::cout << fmt::format("[{:x}-{:x}] ", lo, hi);
    }
    std::cout << "\n";
  }

  int idx = 0;
  for (ir::IrBlock const& block : irg._blocks) {
    std::cout << fmt::format("IR for block {}\n", idx++);
    for (ir::IrInst const& inst : block._insts) {
      std::cout << "    ";
      switch (inst._opc) {
        case ir::IrOpcode::kMov:
          std::cout << "mov";
          break;

        case ir::IrOpcode::kStore:
          std::cout << "store";
          break;

        case ir::IrOpcode::kLoad:
          std::cout << "load";
          break;

        case ir::IrOpcode::kCmp:
          std::cout << "cmp";
          break;

        case ir::IrOpcode::kRcTest:
          std::cout << "rcTest";
          break;

        case ir::IrOpcode::kCall:
          std::cout << "call";
          break;

        case ir::IrOpcode::kReturn:
          std::cout << "return";
          break;

        case ir::IrOpcode::kLsh:
          std::cout << "lsh";
          break;

        case ir::IrOpcode::kRsh:
          std::cout << "rsh";
          break;

        case ir::IrOpcode::kRol:
          std::cout << "rol";
          break;

        case ir::IrOpcode::kRor:
          std::cout << "ror";
          break;

        case ir::IrOpcode::kAndB:
          std::cout << "andB";
          break;

        case ir::IrOpcode::kOrB:
          std::cout << "orB";
          break;

        case ir::IrOpcode::kXorB:
          std::cout << "xorB";
          break;

        case ir::IrOpcode::kNotB:
          std::cout << "notB";
          break;

        case ir::IrOpcode::kAdd:
          std::cout << "add";
          break;

        case ir::IrOpcode::kAddc:
          std::cout << "addc";
          break;

        case ir::IrOpcode::kSub:
          std::cout << "sub";
          break;

        case ir::IrOpcode::kMul:
          std::cout << "mul";
          break;

        case ir::IrOpcode::kDiv:
          std::cout << "div";
          break;

        case ir::IrOpcode::kNeg:
          std::cout << "neg";
          break;

        case ir::IrOpcode::kSqrt:
          std::cout << "sqrt";
          break;

        case ir::IrOpcode::kAbs:
          std::cout << "abs";
          break;

        case ir::IrOpcode::kIntrinsic:
          std::cout << "intrinsic";
          break;

        case ir::IrOpcode::kOptBarrier:
          std::cout << "optBarrier";
          break;
      }

      std::cout << " ";
      for (ir::OpVar ov : inst._ops) {
        std::visit(overloaded{
          [](ir::TVRef tv) { std::cout << fmt::format("t{}", static_cast<uint32_t>(tv._idx)); },
          [](ir::MemRef mr) { std::cout << fmt::format("[t{} + {:x}]", mr._gpr_tv, mr._off); },
          [](ir::StackRef sr) { std::cout << fmt::format("var_{:x}", sr._off); },
          [](ir::ParamRef pr) { std::cout << fmt::format("param_{:x}", pr._off); },
          [](ir::Immediate imm) {
            if (imm._signed) {
              std::cout << fmt::format("{:x}", static_cast<int32_t>(imm._val));
            } else {
              std::cout << fmt::format("{:x}", static_cast<uint32_t>(imm._val));
            }
          },
          [](ir::FunctionRef fr) { std::cout << fmt::format("func_{:x}", fr._func_va); },
          [](ir::ConditionRef cr) { std::cout << fmt::format("crb_{:x}", cr._bits); },
        }, ov);
        std::cout << " ";
      }
      std::cout << "\n";
    }
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

  subroutine._graph = create_graph(*ctx._ram, analysis_start);
  run_liveness_analysis(subroutine._graph, ctx);
  subroutine._stack.run_stack_analysis(subroutine._graph);

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
  std::cout << fmt::format("Stack information:\n  Stack size: 0x{:x}\n", subroutine._stack.stack_size());
  for (StackVariable const& sv : subroutine._stack.var_list()) {
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

    RegisterLiveness* bbp = cur->_block_lifetimes;
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

  run_perilogue_analysis(subroutine, ctx);
  ir::IrGraph irg = ir::translate_subroutine(subroutine);

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
