#include <vector>

#include "Commands.hh"
#include "utl/LaunchCommand.hh"

namespace {
using namespace decomp;

std::vector<LaunchCommand> sCommands = {
  LaunchCommand{
    "testcmd",
    "Scratch command for whatever im doing",
    {},
    {},
    test_cmd,
  },
  LaunchCommand{
    "summarize",
    "Print out a summary of information about a subroutine starting at a specified address",
    {
      ParamDesc{
        "binpath",
        "Path to the executable to be analyzed (DOL, REL, ELF)",
        CommandParamType::kPath,
      },
      ParamDesc{
        "addr",
        "Virtual address of the subroutine to summarize",
        CommandParamType::kU32Hex,
      },
    },
    {},  // TODO: add switches to include things to summarize
    summarize_subroutine,
  },
  LaunchCommand{
    "graphviz",
    "Create a DOT file to visualize a subroutine's basic blocks using Graphviz",
    {
      ParamDesc{
        "binpath",
        "Path to the executable to be visualized (DOL, REL, ELF)",
        CommandParamType::kPath,
      },
      ParamDesc{
        "addr",
        "Virtual address of the subroutine to visualize",
        CommandParamType::kU32Hex,
      },
    },
    {
      OptionDesc{
        "out",
        'o',
        "Explicit output path for DOT file. If not provided, it will be written to '<cwd>/<DOL "
        "name>_<address>.dot'",
        CommandParamType::kPath,
        std::string(),
      },
    },
    dump_dotfile,
  },
  LaunchCommand{
    "sections",
    "Print out a list of all sections from the specified DOL",
    {
      ParamDesc{
        "binpath",
        "Path to the executable to be analyzed (DOL only)",
        CommandParamType::kPath,
      },
    },
    {},
    print_sections,
  },
  LaunchCommand{
    "dis",
    "Print out a linear disassembly starting at a specified address",
    {
      ParamDesc{
        "binpath",
        "Path to the executable to be disassembled (DOL, REL, ELF)",
        CommandParamType::kPath,
      },
      ParamDesc{
        "addr",
        "Virtual address to start disassembly from",
        CommandParamType::kU32Hex,
      },
      ParamDesc{
        "len",
        "Number of instructions to disassemble",
        CommandParamType::kU32,
      },
    },
    {},
    linear_dis,
  },
};
}  // namespace

// clang-format off
int main(int argc, char** argv) {
  return exec_command(sCommands, argc, argv);
}
// clang-format on
