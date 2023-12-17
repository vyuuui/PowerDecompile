#pragma once

#include "ir/GekkoTranslator.hh"

namespace decomp::hll {
struct CFAnalysisSummary {
  
};

struct IfSeq {
};

struct IfElseSeq {
};

struct WhileSeq {
};

struct BreakSeq {
};

void run_control_flow_analysis(ir::IrRoutine const& routine);
}  // namespace decomp::hll
