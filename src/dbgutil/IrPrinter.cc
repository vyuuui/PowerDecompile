#include "dbgutil/IrPrinter.hh"

#include <fmt/format.h>

#include <string>

#include "utl/VariantOverloaded.hh"

namespace decomp {
namespace {
using namespace ir;

std::string opcode_name(IrOpcode opc) {
  switch (opc) {
    case IrOpcode::kMov:
      return "mov";
    case IrOpcode::kStore:
      return "store";
    case IrOpcode::kLoad:
      return "load";
    case IrOpcode::kCmp:
      return "cmp";
    case IrOpcode::kRcTest:
      return "rctest";
    case IrOpcode::kCall:
      return "call";
    case IrOpcode::kReturn:
      return "ret";
    case IrOpcode::kLsh:
      return "lsh";
    case IrOpcode::kRsh:
      return "rsh";
    case IrOpcode::kRol:
      return "rol";
    case IrOpcode::kRor:
      return "ror";
    case IrOpcode::kAndB:
      return "and";
    case IrOpcode::kOrB:
      return "or";
    case IrOpcode::kXorB:
      return "xor";
    case IrOpcode::kNotB:
      return "not";
    case IrOpcode::kAdd:
      return "add";
    case IrOpcode::kAddc:
      return "addc";
    case IrOpcode::kSub:
      return "sub";
    case IrOpcode::kMul:
      return "mul";
    case IrOpcode::kDiv:
      return "div";
    case IrOpcode::kNeg:
      return "neg";
    case IrOpcode::kSqrt:
      return "sqrt";
    case IrOpcode::kAbs:
      return "abs";
    case IrOpcode::kIntrinsic:
      return "intrin";
    case IrOpcode::kOptBarrier:
      return "optbarrier";
    default:
      return "invalid";
  }
}

std::string opvar_string(OpVar const& op) {
  return std::visit(overloaded{
                      [](TempVar tv) {
                        switch (tv._base._class) {
                          case TempClass::kIntegral:
                            return fmt::format("int_tmp{}", tv._base._idx);
                          case TempClass::kFloating:
                            return fmt::format("float_tmp{}", tv._base._idx);
                          case TempClass::kCondition:
                            switch (tv._cnd._condbits) {
                              case 0xf:
                                return fmt::format("cond_tmp{}", tv._cnd._idx);
                              case 0x1:
                                return fmt::format("cond_tmp{}.lt", tv._cnd._idx);
                              case 0x2:
                                return fmt::format("cond_tmp{}.gt", tv._cnd._idx);
                              case 0x4:
                                return fmt::format("cond_tmp{}.eq", tv._cnd._idx);
                              case 0x8:
                                return fmt::format("cond_tmp{}.so", tv._cnd._idx);
                              default:
                                return fmt::format("cond_tmp{}.{}", tv._cnd._idx, tv._cnd._condbits);
                            }
                          default:
                            return std::string();
                        }
                      },
                      [](MemRef mr) { return fmt::format("[int_tmp{} + 0x{:x}]", mr._gpr_tv, mr._off); },
                      [](StackRef sr) { return fmt::format("{}var_{:x}", sr._addrof ? "&" : "", sr._off); },
                      [](ParamRef pr) { return fmt::format("{}param_{}", pr._addrof ? "&" : "", pr._param_idx); },
                      [](Immediate imm) {
                        if (imm._signed) {
                          return std::to_string(static_cast<int32_t>(imm._val));
                        }
                        return std::to_string(imm._val);
                      },
                      [](FunctionRef fr) { return fmt::format("sub_{:x}", fr._func_va); },
                    },
    op);
}
}  // namespace

void write_ir_inst(IrInst const& inst, std::ostream& sink) {
  sink << opcode_name(inst._opc);
  for (OpVar const& op : inst._ops) {
    sink << " " << opvar_string(op);
  }
}

void write_block_transition(BlockTransition const& transition, std::ostream& sink) {
  sink << fmt::format("jmp to block {} ", transition._target_idx);
  if (transition._cond && transition._counter == CounterCheck::kIgnore) {
    std::string cond_str = opvar_string(TempVar{._cnd = *transition._cond});
    sink << fmt::format("if {} is {}", cond_str, transition._take_if_true ^ transition._inv_cond ? "true" : "false");
  } else if (transition._cond && transition._counter != CounterCheck::kIgnore) {
    std::string cond_str = opvar_string(TempVar{._cnd = *transition._cond});
    sink << fmt::format("if {} is {} ", cond_str, transition._take_if_true ^ transition._inv_cond ? "true" : "false");
    sink << fmt::format("and CTR is {}", transition._counter == CounterCheck::kCounterZero ? "zero" : "not zero");
  } else if (!transition._cond && transition._counter != CounterCheck::kIgnore) {
    sink << fmt::format("if CTR is {}", transition._counter == CounterCheck::kCounterZero ? "zero" : "not zero");
  } else {
    sink << "unconditionally";
  }
}

void write_block(IrBlock const& block, std::ostream& sink) {
  sink << fmt::format("Start of block id {}\n", block._id);
  for (IrInst const& inst : block._insts) {
    write_ir_inst(inst, sink);
    sink << "\n";
  }
  for (BlockTransition const& transition : block._tr_out) {
    write_block_transition(transition, sink);
    sink << "\n";
  }
}
}  // namespace decomp
