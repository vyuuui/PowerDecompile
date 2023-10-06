#pragma once

namespace decomp {
class CommandParamList;

int summarize_subroutine(CommandParamList const&);
int dump_dotfile(CommandParamList const&);
int print_sections(CommandParamList const&);
int linear_dis(CommandParamList const&);
}  // namespace decomp
