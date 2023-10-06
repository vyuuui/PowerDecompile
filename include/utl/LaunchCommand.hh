#pragma once

#include <assert.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

// LaunchCommand: System for organizing command line launch into verbs with a defined set of parameters
// Parameters are parsed before execution of command function to ensure correctness of input
namespace decomp {
class CommandParamList;
using CmdParamVariant = std::variant<std::monostate, std::string, uint32_t, bool>;

enum class CommandParamType { kPath, kU32, kU32Hex, kBoolean };

constexpr const char* kDefaultProgName = "ppc-decomp";

// Required parameters, have a defined order of appearance
struct ParamDesc {
  std::string_view _name;
  std::string_view _desc;
  CommandParamType _type;
};

// Optional parameters with default values, appear in any order after primary parameters
// Denoted with -shortopt or --opt, followed by the param value (with an optional =)
// boolean flags are not followed by a value, they are either present or absent (true or false)
struct OptionDesc {
  std::string_view _opt;
  char _shortopt;
  std::string_view _desc;
  CommandParamType _type;
  CmdParamVariant _default;
};

struct LaunchCommand {
  std::string_view _name;
  std::string_view _desc;
  std::vector<ParamDesc> _params;
  std::vector<OptionDesc> _opts;

  int (*command_fn)(CommandParamList const&);

  OptionDesc const* find_opt(std::string_view opt) const;
  OptionDesc const* find_opt(char shortopt) const;
};

class CommandParamList {
private:
  struct ParseState {
    char* _curs;
    char** _cur_argv;
    void nxt_arg() { _curs = *++_cur_argv; }
    constexpr bool at_end() const { return _curs == nullptr; }
  };

private:
  LaunchCommand const& _cmd;
  std::vector<CmdParamVariant> _parsed_args;
  std::vector<std::tuple<char, std::string_view, CmdParamVariant>> _parsed_opts;
  const CmdParamVariant _invalid;

private:
  std::optional<std::string> parse_arg(ParseState& st, ParamDesc const& desc);
  std::optional<std::string> parse_opt(ParseState& st, OptionDesc const& desc);
  std::optional<std::string> parse_argv(char** argv);

  friend int exec_command(std::vector<LaunchCommand> const& cmd_list, int argc, char** argv);

public:
  CommandParamList(LaunchCommand const& cmd) : _cmd(cmd), _invalid(std::monostate()) {}

  CmdParamVariant const& param(size_t index) const;
  template <typename T>
  T const& param_v(size_t index) const {
    return std::get<T>(param(index));
  }

  CmdParamVariant const& option(std::string_view opt) const;
  template <typename T>
  T const& option_v(std::string_view opt) const {
    return std::get<T>(option(opt));
  }

  CmdParamVariant const& option(char shortopt) const;
  template <typename T>
  T const& option_v(char shortopt) const {
    return std::get<T>(option(shortopt));
  }
};

int exec_command(std::vector<LaunchCommand> const& cmd_list, int argc, char** argv);
}  // namespace decomp
