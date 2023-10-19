#pragma once

#include "utl/LaunchCommand.hh"

namespace decomp {
int test_cmd(CommandParamList const&);
int summarize_subroutine(CommandParamList const&);
int dump_dotfile(CommandParamList const&);
int print_sections(CommandParamList const&);
int linear_dis(CommandParamList const&);
}  // namespace decomp
