#include "ir/SSATranslator.hh"

namespace decomp::ir {
namespace {
using namespace ppc;

void translate_ssa_block(BasicBlockVertex const& ppc, SSABlockVertex& ssa) {
  BasicBlock const& bb = ppc.data();
  SSABlock& ssabb = ssa.data();

  const auto w1r1 = [&ssabb](MetaInst const& inst,
                      SSAUnaryExpr op,
                      DataSize sz = DataSize::kSInfer,
                      SignednessHint hint = SignednessHint::kNoHint) {
    ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write, hint),
      new UnaryExpr(op, SSATempVar::from_datasource(inst._reads[0], hint), sz, hint)));
  };

  const auto w1r2 = [&ssabb](MetaInst const& inst,
                      SSABinaryExpr op,
                      DataSize sz = DataSize::kSInfer,
                      SignednessHint hint = SignednessHint::kNoHint) {
    ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write, hint),
      new BinaryExpr(op,
        SSATempVar::from_datasource(inst._reads[0], hint),
        SSATempVar::from_datasource(inst._reads[1], hint),
        sz,
        hint)));
  };

  const auto w1r3 = [&ssabb](MetaInst const& inst,
                      SSABinaryExpr op,
                      DataSize sz = DataSize::kSInfer,
                      SignednessHint hint = SignednessHint::kNoHint) {
    ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write, hint),
      new BinaryExpr(op,
        new BinaryExpr(
          op, SSATempVar::from_datasource(inst._reads[0], hint), SSATempVar::from_datasource(inst._reads[1], hint)),
        SSATempVar::from_datasource(inst._reads[2], hint),
        sz,
        hint)));
  };

  const auto oerc = [&ssabb](MetaInst const& inst, DataSize sz = DataSize::kSInfer) {
    if (check_flags(inst._sidefx, InstSideFx::kWritesOVSO)) {
      // TODO: track overflow (can't find any code that generates this flag)
    }
    if (check_flags(inst._sidefx, InstSideFx::kWritesRecord)) {
      // Breaking CR fields into their component bits allows for more precise propagation of comparisons
      ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(CRBit::kCr0Gt),
        new BinaryExpr(SSABinaryExpr::kCmpGrtr,
          SSATempVar::from_datasource(*inst._write, SignednessHint::kSigned),
          new IntegerImm(0, DataSize::kS32, SignednessHint::kSigned))));
      ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(CRBit::kCr0Lt),
        new BinaryExpr(SSABinaryExpr::kCmpLess,
          SSATempVar::from_datasource(*inst._write, SignednessHint::kSigned),
          new IntegerImm(0, DataSize::kS32, SignednessHint::kSigned))));
      ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(CRBit::kCr0Eq),
        new BinaryExpr(SSABinaryExpr::kCmpEq,
          SSATempVar::from_datasource(*inst._write, SignednessHint::kSigned),
          new IntegerImm(0, DataSize::kS32, SignednessHint::kSigned))));
      // TODO: Add some weak-usage flag that hints to later reductions that this usage does not extend the liveness
      // of the source (XER[SO] is rarely ever used, and is likely undefined at any given point in time)
      ssabb._insts.push_back(new AssignmentStatement(
        new SSATempVar(CRBit::kCr0So), new SSATempVar("XER[SO]", DataSize::kS1, SignednessHint::kUnsigned)));
    }
    if (check_flags(inst._sidefx, InstSideFx::kWritesFpRecord)) {
      // TODO: Track FP exception state to CR1
    }
  };

  const auto update_base = [&ssabb](MetaInst const& inst, ppc::DataSource const& mem) {
    if (check_flags(inst._sidefx, InstSideFx::kWritesBaseReg)) {
      if (MemRegReg const* mrr = std::get_if<MemRegReg>(&mem); mrr != nullptr) {
        ssabb._insts.push_back(new AssignmentStatement(
          new SSATempVar(GPRSlice{._reg = mrr->_base, ._type = DataType::kS4}, SignednessHint::kUnsigned),
          new BinaryExpr(SSABinaryExpr::kAdd,
            new SSATempVar(GPRSlice{._reg = mrr->_base, ._type = DataType::kS4}, SignednessHint::kUnsigned),
            new SSATempVar(GPRSlice{._reg = mrr->_offset, ._type = DataType::kS4}, SignednessHint::kUnsigned))));
      } else if (MemRegOff const* mro = std::get_if<MemRegOff>(&mem); mro != nullptr) {
        ssabb._insts.push_back(new AssignmentStatement(
          new SSATempVar(GPRSlice{._reg = mro->_base, ._type = DataType::kS4}, SignednessHint::kUnsigned),
          new BinaryExpr(SSABinaryExpr::kAdd,
            new SSATempVar(GPRSlice{._reg = mro->_base, ._type = DataType::kS4}, SignednessHint::kUnsigned),
            new IntegerImm(mro->_offset, DataSize::kS32, SignednessHint::kUnsigned))));
      } else {
        assert(false);
      }
    }
  };

  for (auto inst : bb._instructions) {
    switch (inst._op) {
      case InstOperation::kAdd:
      case InstOperation::kAddi: {
        w1r2(inst, SSABinaryExpr::kAdd);
        oerc(inst);
        break;
      }

      case InstOperation::kAddc:
      case InstOperation::kAddic:
      case InstOperation::kAddicDot: {
        w1r2(inst, SSABinaryExpr::kAdd);
        // TODO: track carry
        oerc(inst);
        break;
      }

      case InstOperation::kAdde:
        w1r3(inst, SSABinaryExpr::kAdd);
        // TODO: track carry
        oerc(inst);
        break;

      // Need to shift the immediate left 16 bits
      case InstOperation::kAddis: {
        const uint32_t shifted = static_cast<uint32_t>(std::get<SIMM>(inst._reads[1])._imm_value) << 16;
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kAdd,
            SSATempVar::from_datasource(inst._reads[0]),
            new IntegerImm(shifted, DataSize::kS32))));
        break;
      }

      case InstOperation::kAddme: {
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kSub,
            new BinaryExpr(SSABinaryExpr::kAdd,
              SSATempVar::from_datasource(inst._reads[0]),
              SSATempVar::from_datasource(inst._reads[1])),
            new IntegerImm(1))));
        // TODO: track carry
        oerc(inst);
        break;
      }

      case InstOperation::kAddze:
        w1r2(inst, SSABinaryExpr::kAdd, DataSize::kS32);
        // TODO: track carry
        oerc(inst);
        break;

      case InstOperation::kDivw:
        w1r2(inst, SSABinaryExpr::kDiv, DataSize::kSInfer, SignednessHint::kSigned);
        oerc(inst);
        break;

      case InstOperation::kDivwu:
        w1r2(inst, SSABinaryExpr::kDiv, DataSize::kSInfer, SignednessHint::kUnsigned);
        oerc(inst);
        break;

      case InstOperation::kMulhw:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new UnaryExpr(SSAUnaryExpr::kHigh,
            new BinaryExpr(SSABinaryExpr::kMul,
              SSATempVar::from_datasource(inst._reads[0]),
              SSATempVar::from_datasource(inst._reads[1]),
              DataSize::kS64,
              SignednessHint::kSigned))));
        oerc(inst);
        break;

      case InstOperation::kMulhwu:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new UnaryExpr(SSAUnaryExpr::kHigh,
            new BinaryExpr(SSABinaryExpr::kMul,
              SSATempVar::from_datasource(inst._reads[0]),
              SSATempVar::from_datasource(inst._reads[1]),
              DataSize::kS64,
              SignednessHint::kUnsigned))));
        oerc(inst);
        break;

      case InstOperation::kMulli:
      case InstOperation::kMullw:
        w1r2(inst, SSABinaryExpr::kMul);
        oerc(inst);
        break;

      case InstOperation::kNeg:
        w1r1(inst, SSAUnaryExpr::kNeg, DataSize::kSInfer, SignednessHint::kSigned);
        oerc(inst);
        break;

      case InstOperation::kSubf:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kSub,
            SSATempVar::from_datasource(inst._reads[1]),
            SSATempVar::from_datasource(inst._reads[0]))));
        oerc(inst);
        break;

      case InstOperation::kSubfc:
      case InstOperation::kSubfic:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kSub,
            SSATempVar::from_datasource(inst._reads[1]),
            SSATempVar::from_datasource(inst._reads[0]))));
        // TODO: track carry
        oerc(inst);
        break;

      case InstOperation::kSubfe:
        // w = r1 - r0 + CA - 1
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kSub,
            new BinaryExpr(SSABinaryExpr::kAdd,
              new BinaryExpr(SSABinaryExpr::kSub,
                SSATempVar::from_datasource(inst._reads[1]),
                SSATempVar::from_datasource(inst._reads[0])),
              SSATempVar::from_datasource(inst._reads[2])),
            new IntegerImm(1))));
        // TODO: track carry
        oerc(inst);
        break;
      case InstOperation::kSubfme:
        // w = CA - r0 - 2
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kSub,
            new BinaryExpr(SSABinaryExpr::kSub,
              SSATempVar::from_datasource(inst._reads[1]),
              SSATempVar::from_datasource(inst._reads[0])),
            new IntegerImm(2))));
        // TODO: track carry
        oerc(inst);
        break;

      case InstOperation::kSubfze:
        // w = CA - r0 - 1
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kSub,
            new BinaryExpr(SSABinaryExpr::kSub,
              SSATempVar::from_datasource(inst._reads[1]),
              SSATempVar::from_datasource(inst._reads[0])),
            new IntegerImm(1))));
        // TODO: track carry
        oerc(inst);
        break;

      case InstOperation::kCmp:
      case InstOperation::kCmpi:
        ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(gt_bit(inst._binst.crfd())),
          new BinaryExpr(SSABinaryExpr::kCmpGrtr,
            SSATempVar::from_datasource(inst._reads[0], SignednessHint::kSigned),
            SSATempVar::from_datasource(inst._reads[1], SignednessHint::kSigned))));
        ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(lt_bit(inst._binst.crfd())),
          new BinaryExpr(SSABinaryExpr::kCmpLess,
            SSATempVar::from_datasource(inst._reads[0], SignednessHint::kSigned),
            SSATempVar::from_datasource(inst._reads[1], SignednessHint::kSigned))));
        ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(eq_bit(inst._binst.crfd())),
          new BinaryExpr(SSABinaryExpr::kCmpEq,
            SSATempVar::from_datasource(inst._reads[0], SignednessHint::kSigned),
            SSATempVar::from_datasource(inst._reads[1], SignednessHint::kSigned))));
        ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(so_bit(inst._binst.crfd())),
          new SSATempVar("XER[SO]", DataSize::kS1, SignednessHint::kUnsigned)));
        break;

      case InstOperation::kCmpl:
      case InstOperation::kCmpli:
        ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(gt_bit(inst._binst.crfd())),
          new BinaryExpr(SSABinaryExpr::kCmpGrtr,
            SSATempVar::from_datasource(inst._reads[0], SignednessHint::kUnsigned),
            SSATempVar::from_datasource(inst._reads[1], SignednessHint::kUnsigned))));
        ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(lt_bit(inst._binst.crfd())),
          new BinaryExpr(SSABinaryExpr::kCmpLess,
            SSATempVar::from_datasource(inst._reads[0], SignednessHint::kUnsigned),
            SSATempVar::from_datasource(inst._reads[1], SignednessHint::kUnsigned))));
        ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(eq_bit(inst._binst.crfd())),
          new BinaryExpr(SSABinaryExpr::kCmpEq,
            SSATempVar::from_datasource(inst._reads[0], SignednessHint::kUnsigned),
            SSATempVar::from_datasource(inst._reads[1], SignednessHint::kUnsigned))));
        ssabb._insts.push_back(new AssignmentStatement(new SSATempVar(so_bit(inst._binst.crfd())),
          new SSATempVar("XER[SO]", DataSize::kS1, SignednessHint::kUnsigned)));
        break;

      case InstOperation::kAnd:
      case InstOperation::kAndiDot:
        w1r2(inst, SSABinaryExpr::kAndB, DataSize::kSInfer, SignednessHint::kUnsigned);
        oerc(inst);
        break;

      case InstOperation::kAndc: {
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kAndB,
            SSATempVar::from_datasource(inst._reads[0], SignednessHint::kUnsigned),
            new UnaryExpr(SSAUnaryExpr::kNotB, SSATempVar::from_datasource(inst._reads[1], SignednessHint::kUnsigned)),
            DataSize::kSInfer,
            SignednessHint::kUnsigned)));
        oerc(inst);
        break;
      }

      case InstOperation::kAndisDot: {
        const uint32_t shifted = static_cast<uint32_t>(std::get<SIMM>(inst._reads[1])._imm_value) << 16;
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kAndB,
            SSATempVar::from_datasource(inst._reads[0]),
            new IntegerImm(shifted, DataSize::kS32))));
        oerc(inst);
        break;
      }

      case InstOperation::kCntlzw: {
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new IntrinsicExpr("cntlzw", SSATempVar::from_datasource(inst._reads[0]), nullptr)));
        oerc(inst);
        break;
      }

      case InstOperation::kEqv:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new UnaryExpr(SSAUnaryExpr::kNotB,
            new BinaryExpr(SSABinaryExpr::kXorB,
              SSATempVar::from_datasource(inst._reads[0]),
              SSATempVar::from_datasource(inst._reads[1])))));
        oerc(inst);
        break;

      case InstOperation::kExtsb:
      case InstOperation::kExtsh:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new UnaryExpr(SSAUnaryExpr::kSizeCast,
            SSATempVar::from_datasource(inst._reads[0], SignednessHint::kSigned),
            DataSize::kS32,
            SignednessHint::kSigned)));
        oerc(inst);
        break;

      case InstOperation::kNand:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new UnaryExpr(SSAUnaryExpr::kNotB,
            new BinaryExpr(SSABinaryExpr::kAndB,
              SSATempVar::from_datasource(inst._reads[0]),
              SSATempVar::from_datasource(inst._reads[1])))));
        oerc(inst);
        break;

      case InstOperation::kNor:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new UnaryExpr(SSAUnaryExpr::kNotB,
            new BinaryExpr(SSABinaryExpr::kOrB,
              SSATempVar::from_datasource(inst._reads[0]),
              SSATempVar::from_datasource(inst._reads[1])))));
        oerc(inst);
        break;

      case InstOperation::kOr:
        if (inst._reads[0] == inst._reads[1]) {
          ssabb._insts.push_back(new AssignmentStatement(
            SSATempVar::from_datasource(*inst._write), SSATempVar::from_datasource(inst._reads[0])));
        } else {
          w1r2(inst, SSABinaryExpr::kOrB);
        }
        oerc(inst);
        break;

      case InstOperation::kOrc:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kOrB,
            SSATempVar::from_datasource(inst._reads[0]),
            new UnaryExpr(SSAUnaryExpr::kNotB, SSATempVar::from_datasource(inst._reads[1])))));
        oerc(inst);
        break;

      case InstOperation::kOri:
        // Standard NOP encoding
        if (inst._binst.ra() == GPR::kR0 && inst._binst.rs() == GPR::kR0 && inst._binst.uimm()._imm_value == 0) {
          break;
        }
        w1r2(inst, SSABinaryExpr::kOrB);
        break;

      case InstOperation::kOris: {
        const uint32_t shifted = static_cast<uint32_t>(std::get<SIMM>(inst._reads[1])._imm_value) << 16;
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kOrB,
            SSATempVar::from_datasource(inst._reads[0]),
            new IntegerImm(shifted, DataSize::kS32))));
        break;
      }

      case InstOperation::kXor:
      case InstOperation::kXori:
        w1r2(inst, SSABinaryExpr::kXorB);
        oerc(inst);
        break;

      case InstOperation::kXoris: {
        const uint32_t shifted = static_cast<uint32_t>(std::get<SIMM>(inst._reads[1])._imm_value) << 16;
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kXorB,
            SSATempVar::from_datasource(inst._reads[0]),
            new IntegerImm(shifted, DataSize::kS32))));
        break;
      }

      case InstOperation::kRlwimi: {
        SimplifiedRlwimi shorthand = inst.simplified_rlwimi();
        switch (shorthand._op) {
          case RlwimiOp::kNone:
            // There's a good reason we want simplified versions of this trash
            ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
              new BinaryExpr(SSABinaryExpr::kOrB,
                new BinaryExpr(SSABinaryExpr::kAndB,
                  new BinaryExpr(SSABinaryExpr::kRol,
                    SSATempVar::from_datasource(inst._reads[0]),
                    new IntegerImm(inst._binst.sh()._val)),
                  new IntegerImm(gen_mask(inst._binst.mb()._val, inst._binst.me()._val))),
                new BinaryExpr(SSABinaryExpr::kAndB,
                  SSATempVar::from_datasource(inst._reads[1]),
                  new IntegerImm(~gen_mask(inst._binst.mb()._val, inst._binst.me()._val))))));
            break;

          case RlwimiOp::kInslwi:
            ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
              new BinaryExpr(SSABinaryExpr::kOrB,
                new BinaryExpr(SSABinaryExpr::kAndB,
                  new BinaryExpr(
                    SSABinaryExpr::kRsh, SSATempVar::from_datasource(inst._reads[0]), new IntegerImm(shorthand._b)),
                  new IntegerImm(gen_mask(shorthand._b, shorthand._b + shorthand._n - 1))),
                new BinaryExpr(SSABinaryExpr::kAndB,
                  SSATempVar::from_datasource(inst._reads[1]),
                  new IntegerImm(~gen_mask(shorthand._b, shorthand._b + shorthand._n - 1))))));
            break;

          case RlwimiOp::kInsrwi:
            ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
              new BinaryExpr(SSABinaryExpr::kOrB,
                new BinaryExpr(SSABinaryExpr::kAndB,
                  new BinaryExpr(
                    SSABinaryExpr::kLsh, SSATempVar::from_datasource(inst._reads[0]), new IntegerImm(shorthand._b)),
                  new IntegerImm(gen_mask(shorthand._b, shorthand._b + shorthand._n - 1))),
                new BinaryExpr(SSABinaryExpr::kAndB,
                  SSATempVar::from_datasource(inst._reads[1]),
                  new IntegerImm(~gen_mask(shorthand._b, shorthand._b + shorthand._n - 1))))));
            break;
        }
        break;
      }

      case InstOperation::kRlwinm: {
        SimplifiedRlwinm shorthand = inst.simplified_rlwinm();
        switch (shorthand._op) {
          case RlwinmOp::kNone:
            ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
              new BinaryExpr(SSABinaryExpr::kAndB,
                new BinaryExpr(SSABinaryExpr::kRol,
                  SSATempVar::from_datasource(inst._reads[0]),
                  new IntegerImm(inst._binst.sh()._val)),
                new IntegerImm(gen_mask(inst._binst.mb()._val, inst._binst.me()._val)))));
            break;

          case RlwinmOp::kExtlwi:
            ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
              new BinaryExpr(SSABinaryExpr::kAndB,
                new BinaryExpr(SSABinaryExpr::kRol,
                  SSATempVar::from_datasource(inst._reads[0]),
                  new IntegerImm(inst._binst.sh()._val)),
                new IntegerImm(gen_mask(shorthand._b, shorthand._b + shorthand._n - 1)))));
            break;

          case RlwinmOp::kExtrwi:
            break;

          case RlwinmOp::kRotlwi:
            break;

          case RlwinmOp::kRotrwi:
            break;

          case RlwinmOp::kSlwi:
            break;

          case RlwinmOp::kSrwi:
            // unsigned
            break;

          case RlwinmOp::kClrlwi:
            break;

          case RlwinmOp::kClrrwi:
            break;

          case RlwinmOp::kClrlslwi:
            break;
        }
        break;
      }

      case InstOperation::kRlwnm: {
        SimplifiedRlwnm shorthand = inst.simplified_rlwnm();
        switch (shorthand._op) {
          case RlwnmOp::kNone:
            break;

          case RlwnmOp::kRotlw:
            break;
        }
        break;
      }

      case InstOperation::kSlw:
        w1r2(inst, SSABinaryExpr::kLsh);
        oerc(inst);
        break;

      case InstOperation::kSraw:
      case InstOperation::kSrawi:
        w1r2(inst, SSABinaryExpr::kRsh, DataSize::kSInfer, SignednessHint::kSigned);
        oerc(inst);
        break;

      case InstOperation::kSrw:
        w1r2(inst, SSABinaryExpr::kRsh, DataSize::kSInfer, SignednessHint::kUnsigned);
        oerc(inst);
        break;

      case InstOperation::kFadd:
        w1r2(inst, SSABinaryExpr::kAdd, DataSize::kS64, SignednessHint::kSigned);
        oerc(inst, DataSize::kS64);
        break;

      case InstOperation::kFadds:
        w1r2(inst, SSABinaryExpr::kAdd, DataSize::kS32, SignednessHint::kSigned);
        oerc(inst, DataSize::kS32);
        break;

      case InstOperation::kFdiv:
        w1r2(inst, SSABinaryExpr::kDiv, DataSize::kS64, SignednessHint::kSigned);
        oerc(inst, DataSize::kS64);
        break;

      case InstOperation::kFdivs:
        w1r2(inst, SSABinaryExpr::kDiv, DataSize::kS32, SignednessHint::kSigned);
        oerc(inst, DataSize::kS32);
        break;

      case InstOperation::kFmul:
        w1r2(inst, SSABinaryExpr::kMul, DataSize::kS64, SignednessHint::kSigned);
        oerc(inst, DataSize::kS64);
        break;

      case InstOperation::kFmuls:
        w1r2(inst, SSABinaryExpr::kMul, DataSize::kS32, SignednessHint::kSigned);
        oerc(inst, DataSize::kS32);
        break;

      case InstOperation::kFres:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kDiv, new FloatingImm(1.0f), SSATempVar::from_datasource(inst._reads[0]))));
        oerc(inst, DataSize::kS32);
        break;

      case InstOperation::kFrsqrte:
        ssabb._insts.push_back(new AssignmentStatement(SSATempVar::from_datasource(*inst._write),
          new BinaryExpr(SSABinaryExpr::kDiv,
            new FloatingImm(1.0f),
            new UnaryExpr(SSAUnaryExpr::kSqrt, SSATempVar::from_datasource(inst._reads[0])))));
        oerc(inst, DataSize::kS32);
        break;

      case InstOperation::kFsub:
        w1r2(inst, SSABinaryExpr::kSub, DataSize::kS64, SignednessHint::kSigned);
        oerc(inst, DataSize::kS64);
        break;

      case InstOperation::kFsubs:
        w1r2(inst, SSABinaryExpr::kSub, DataSize::kS32, SignednessHint::kSigned);
        oerc(inst, DataSize::kS32);
        break;

      case InstOperation::kFsel:
        // TODO: does this require generating its own basic block?
        assert(false);
        break;

      case InstOperation::kFmadd:
        break;

      case InstOperation::kFmadds:
      case InstOperation::kFmsub:
      case InstOperation::kFmsubs:
      case InstOperation::kFnmadd:
      case InstOperation::kFnmadds:
      case InstOperation::kFnmsub:
      case InstOperation::kFnmsubs:
      case InstOperation::kFctiw:
      case InstOperation::kFctiwz:
      case InstOperation::kFrsp:
      case InstOperation::kFcmpo:
      case InstOperation::kFcmpu:
        break;
        // stuff below to the break untranslatable
      case InstOperation::kMcrfs:
      case InstOperation::kMffs:
      case InstOperation::kMtfsb0:
      case InstOperation::kMtfsb1:
      case InstOperation::kMtfsf:
      case InstOperation::kMtfsfi:
        break;

      // Signed, no-update
      case InstOperation::kLha:
      case InstOperation::kLhax:
      case InstOperation::kLfd:
      case InstOperation::kLfdx:
      case InstOperation::kLfs:
      case InstOperation::kLfsx:
        ssabb._insts.push_back(new AssignmentStatement(
          SSATempVar::from_datasource(*inst._write), new MemExpr(inst._reads[0], SignednessHint::kSigned)));
        break;
      // Unsigned, no-update
      case InstOperation::kLbz:
      case InstOperation::kLbzx:
      case InstOperation::kLhz:
      case InstOperation::kLwz:
      case InstOperation::kLhzx:
      case InstOperation::kLwzx:
        ssabb._insts.push_back(new AssignmentStatement(
          SSATempVar::from_datasource(*inst._write), new MemExpr(inst._reads[0], SignednessHint::kUnsigned)));
        break;
      // Signed, update
      case InstOperation::kLhau:
      case InstOperation::kLhaux:
      case InstOperation::kLfdu:
      case InstOperation::kLfdux:
      case InstOperation::kLfsu:
      case InstOperation::kLfsux:
        ssabb._insts.push_back(new AssignmentStatement(
          SSATempVar::from_datasource(*inst._write), new MemExpr(inst._reads[0], SignednessHint::kSigned)));
        update_base(inst, inst._reads[0]);
        break;
      // Unsigned, update
      case InstOperation::kLbzu:
      case InstOperation::kLbzux:
      case InstOperation::kLhzu:
      case InstOperation::kLwzu:
      case InstOperation::kLhzux:
      case InstOperation::kLwzux:
        ssabb._insts.push_back(new AssignmentStatement(
          SSATempVar::from_datasource(*inst._write), new MemExpr(inst._reads[0], SignednessHint::kUnsigned)));
        update_base(inst, inst._reads[0]);
        break;

      // No-update
      case InstOperation::kStb:
      case InstOperation::kStbx:
      case InstOperation::kSth:
      case InstOperation::kSthx:
      case InstOperation::kStw:
      case InstOperation::kStwx:
      case InstOperation::kStfd:
      case InstOperation::kStfdx:
      case InstOperation::kStfiwx:
      case InstOperation::kStfs:
      case InstOperation::kStfsx:
        ssabb._insts.push_back(new AssignmentStatement(new MemExpr(*inst._write), new MemExpr(inst._reads[0])));
        break;
      // Update
      case InstOperation::kStbu:
      case InstOperation::kStbux:
      case InstOperation::kSthu:
      case InstOperation::kSthux:
      case InstOperation::kStwu:
      case InstOperation::kStwux:
      case InstOperation::kStfdu:
      case InstOperation::kStfdux:
      case InstOperation::kStfsu:
      case InstOperation::kStfsux:
        ssabb._insts.push_back(new AssignmentStatement(new MemExpr(*inst._write), new MemExpr(inst._reads[0])));
        update_base(inst, *inst._write);
        break;
      case InstOperation::kLhbrx:
      case InstOperation::kLwbrx:
      case InstOperation::kSthbrx:
      case InstOperation::kStwbrx:
        // TODO:
        break;

      case InstOperation::kLmw:
      case InstOperation::kStmw:
        // TODO:
        break;

      case InstOperation::kLswi:
      case InstOperation::kLswx:
      case InstOperation::kStswi:
      case InstOperation::kStswx:

      case InstOperation::kEieio:
      case InstOperation::kIsync:
      case InstOperation::kLwarx:
      case InstOperation::kStwcxDot:
      case InstOperation::kSync:

      case InstOperation::kFabs:
        ssabb._insts.emplace_back(SSAOpcode::kAbs,
          SSAOperand(inst._writes[0], DataSize::kS64),
          SSAOperand(inst._reads[0], DataSize::kS64),
          SignednessHint::kSigned);
        oerc(inst, DataSize::kS64);
      case InstOperation::kFmr:
      case InstOperation::kFnabs:
      case InstOperation::kFneg:
        ssabb._insts.emplace_back(SSAOpcode::kNeg,
          SSAOperand(inst._writes[0], DataSize::kS64),
          SSAOperand(inst._reads[0], DataSize::kS64),
          SignednessHint::kSigned);
        oerc(inst, DataSize::kS64);
      case InstOperation::kB:
      case InstOperation::kBc:
      case InstOperation::kBcctr:
      case InstOperation::kBclr:
      case InstOperation::kCrand:
      case InstOperation::kCrandc:
      case InstOperation::kCreqv:
      case InstOperation::kCrnand:
      case InstOperation::kCrnor:
        break;
      //
      case InstOperation::kCror:
        wrr(inst, SSAOpcode::kOrB, DataSize::kS32);
        break;

      case InstOperation::kCrorc: {
        SSAOperand temp_bit = SSAOperand::mktemp_cond(DataSize::kS32);
        ssabb._insts.emplace_back(SSAOpcode::kNotB, temp_bit, SSAOperand(inst._reads[1], DataSize::kS32));
        ssabb._insts.emplace_back(SSAOpcode::kOrB,
          SSAOperand(inst._writes[0], DataSize::kS32),
          SSAOperand(inst._reads[0], DataSize::kS32),
          temp_bit);
        break;
      }

      case InstOperation::kCrxor:
      case InstOperation::kMcrf:
      case InstOperation::kRfi:
      case InstOperation::kSc:
      case InstOperation::kTw:
      case InstOperation::kTwi:
      case InstOperation::kMcrxr:
      case InstOperation::kMfcr:
      case InstOperation::kMfmsr:
      case InstOperation::kMfspr:
      case InstOperation::kMftb:
      case InstOperation::kMtcrf:
      case InstOperation::kMtmsr:
      case InstOperation::kMtspr:
      case InstOperation::kDcbf:
      case InstOperation::kDcbi:
      case InstOperation::kDcbst:
      case InstOperation::kDcbt:
      case InstOperation::kDcbtst:
      case InstOperation::kDcbz:
      case InstOperation::kIcbi:
      case InstOperation::kMfsr:
      case InstOperation::kMfsrin:
      case InstOperation::kMtsr:
      case InstOperation::kMtsrin:
      case InstOperation::kTlbie:
      case InstOperation::kTlbsync:
      case InstOperation::kEciwx:
      case InstOperation::kEcowx:
      case InstOperation::kPsq_lx:
      case InstOperation::kPsq_stx:
      case InstOperation::kPsq_lux:
      case InstOperation::kPsq_stux:
      case InstOperation::kPsq_l:
      case InstOperation::kPsq_lu:
      case InstOperation::kPsq_st:
      case InstOperation::kPsq_stu:
      case InstOperation::kPs_div:
      case InstOperation::kPs_sub:
      case InstOperation::kPs_add:
      case InstOperation::kPs_sel:
      case InstOperation::kPs_res:
      case InstOperation::kPs_mul:
      case InstOperation::kPs_rsqrte:
      case InstOperation::kPs_msub:
      case InstOperation::kPs_madd:
      case InstOperation::kPs_nmsub:
      case InstOperation::kPs_nmadd:
      case InstOperation::kPs_neg:
      case InstOperation::kPs_mr:
      case InstOperation::kPs_nabs:
      case InstOperation::kPs_abs:
      case InstOperation::kPs_sum0:
      case InstOperation::kPs_sum1:
      case InstOperation::kPs_muls0:
      case InstOperation::kPs_muls1:
      case InstOperation::kPs_madds0:
      case InstOperation::kPs_madds1:
      case InstOperation::kPs_cmpu0:
      case InstOperation::kPs_cmpo0:
      case InstOperation::kPs_cmpu1:
      case InstOperation::kPs_cmpo1:
      case InstOperation::kPs_merge00:
      case InstOperation::kPs_merge01:
      case InstOperation::kPs_merge10:
      case InstOperation::kPs_merge11:
      case InstOperation::kDcbz_l:
      case InstOperation::kInvalid:
    }
  }
}
}  // namespace

SSATempVar SSATempVar::from_datasource(ppc::DataSource src, SignednessHint hint) {
  if (ppc::GPRSlice* gpr = std::get_if<ppc::GPRSlice>(&src); gpr != nullptr) {
    return new SSATempVar(*gpr, hint);
  } else if (ppc::FPRSlice* fpr = std::get_if<ppc::FPRSlice>(&src); fpr != nullptr) {
    return new SSATempVar(*fpr, hint);
  } else if (ppc::CRBit* crb = std::get_if<ppc::CRBit>(&src); crb != nullptr) {
    return new SSATempVar(*crb);
  } else if (ppc::XERBit* xb = std::get_if<ppc::XERBit>(&src); xb != nullptr) {
    switch (*xb) {
      case ppc::XERBit::kCA:
        return new SSATempVar("XER[CA]", DataSize::kS1);

      case ppc::XERBit::kSO:
        return new SSATempVar("XER[SO]", DataSize::kS1);

      case ppc::XERBit::kOV:
        return new SSATempVar("XER[OV]", DataSize::kS1);

      case ppc::XERBit::kBC:
        return new SSATempVar("XER[BC]", DataSize::kS8);
    }
  }
  assert(false);
  return nullptr;
}

SSARoutine translate_to_ssa(ppc::Subroutine const& routine) {
  // 1. Basic PPC->SSA (copy routine blocks, fill insts with SSA instead of PPC),
  //    do not fill operands
  // 2. Determine the SSA index for each operand of each ppc inst, add operands to SSA insts
  // 3. Insert PHI functions for merged values
  SSARoutine ret;

  // step 1
  ret._graph.copy_shape_generator(routine._graph.get(), translate_ssa_block);
}
}  // namespace decomp::ir
