#include "utl/LaunchCommand.hh"

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

#include "utl/ErrorOr.hh"

namespace decomp {
namespace {
void print_usage(char const* progname, std::vector<LaunchCommand> const& cmd_list) {
  std::cerr << fmt::format(
    "Expected usage\n    {} command [parameters...] [--options...]\nSupported commands:\n", progname);

  size_t longest_name = std::max_element(cmd_list.begin(), cmd_list.end(), [](auto const& l, auto const& r) {
    return l._name.length() < r._name.length();
  })->_name.length();

  for (LaunchCommand const& cmd : cmd_list) {
    std::cerr << std::left << "    " << std::setw(longest_name) << cmd._name << "  " << cmd._desc << "\n";
  }

  std::cerr << fmt::format("Run '{} --help' for more detailed information\n", progname);
}

char const* type_str(CommandParamType type) {
  switch (type) {
    case CommandParamType::kPath:
      return "path";

    case CommandParamType::kU32:
      return "uint32";

    case CommandParamType::kU32Hex:
      return "hex uint32";

    case CommandParamType::kBoolean:
      return "bool";
  }
  return "";
}

void print_cmd_help(char const* progname, LaunchCommand const& cmd, std::ostream& sink) {
  sink << fmt::format("    '{} {}", progname, cmd._name);
  for (ParamDesc const& desc : cmd._params) {
    sink << fmt::format(" [{}]", desc._name);
  }
  for (OptionDesc const& desc : cmd._opts) {
    if (desc._type == CommandParamType::kBoolean) {
      sink << fmt::format(" [--{}]", desc._opt);
    } else {
      sink << fmt::format(" [--{} <{}>]", desc._opt, type_str(desc._type));
    }
  }
  sink << fmt::format("'\n    {}\n", cmd._desc);

  if (!cmd._params.empty()) {
    sink << "Required parameters:\n";
    for (ParamDesc const& desc : cmd._params) {
      sink << fmt::format("    {} ({})\n        {}\n", desc._name, type_str(desc._type), desc._desc);
    }
  }

  if (!cmd._opts.empty()) {
    sink << "Options:\n";
    for (OptionDesc const& desc : cmd._opts) {
      sink << fmt::format(
        "    --{}, -{} ({})\n        {}\n", desc._opt, desc._shortopt, type_str(desc._type), desc._desc);
    }
  }
}

void print_help(char const* progname, std::vector<LaunchCommand> const& cmd_list) {
  std::cout << fmt::format("Usage:  {} command [parameters...] [--options...]\nCommands:\n", progname);
  for (LaunchCommand const& cmd : cmd_list) {
    std::cout << fmt::format("Command '{}'\n", cmd._name);
    print_cmd_help(progname, cmd, std::cout);
    std::cout << '\n';
  }
}

ErrorOr<uint32_t> parse_u32_base(char const* str, int base) {
  char* parse_end;
  uint32_t ret = static_cast<uint32_t>(strtoul(str, &parse_end, 0));
  if (*parse_end != '\0') {
    return fmt::format("Invalid u32, junk at end of '{}' ('{}')", str, parse_end);
  }
  return ret;
}
}  // namespace

OptionDesc const* LaunchCommand::find_opt(std::string_view opt) const {
  auto it = std::find_if(_opts.begin(), _opts.end(), [opt](OptionDesc const& od) { return od._opt == opt; });
  return it == _opts.end() ? nullptr : &*it;
}

OptionDesc const* LaunchCommand::find_opt(char shortopt) const {
  auto it =
    std::find_if(_opts.begin(), _opts.end(), [shortopt](OptionDesc const& od) { return od._shortopt == shortopt; });
  return it == _opts.end() ? nullptr : &*it;
}

std::optional<std::string> CommandParamList::parse_arg(ParseState& st, ParamDesc const& desc) {
  switch (desc._type) {
    case CommandParamType::kPath:
      _parsed_args.emplace_back(std::string(st._curs));
      break;

    case CommandParamType::kU32: {
      auto result = parse_u32_base(st._curs, 0);
      if (result.is_error()) {
        return result.err();
      }
      _parsed_args.emplace_back(result.val());
      break;
    }

    case CommandParamType::kU32Hex: {
      auto result = parse_u32_base(st._curs, 16);
      if (result.is_error()) {
        return result.err();
      }
      _parsed_args.emplace_back(result.val());
      break;
    }

    case CommandParamType::kBoolean:
      assert(false);
      break;
  }

  st.nxt_arg();
  return std::nullopt;
}

std::optional<std::string> CommandParamList::parse_opt(ParseState& st, OptionDesc const& desc) {
  switch (desc._type) {
    case CommandParamType::kPath:
      _parsed_opts.emplace_back(desc._shortopt, desc._opt, CmdParamVariant(std::string(st._curs)));
      st.nxt_arg();
      break;

    case CommandParamType::kU32: {
      auto result = parse_u32_base(st._curs, 0);
      if (result.is_error()) {
        return result.err();
      }
      _parsed_opts.emplace_back(desc._shortopt, desc._opt, CmdParamVariant(result.val()));
      st.nxt_arg();
      break;
    }

    case CommandParamType::kU32Hex: {
      auto result = parse_u32_base(st._curs, 16);
      if (result.is_error()) {
        return result.err();
      }
      _parsed_opts.emplace_back(desc._shortopt, desc._opt, CmdParamVariant(result.val()));
      st.nxt_arg();
      break;
    }

    case CommandParamType::kBoolean:
      _parsed_opts.emplace_back(desc._shortopt, desc._opt, CmdParamVariant(true));
      break;
  }

  return std::nullopt;
}

std::optional<std::string> CommandParamList::parse_argv(char** argv) {
  enum {
    Header,
    Longopt,
    Shortopt,
  } parse_mode = Header;
  ParseState st = {
    ._curs = *argv,
    ._cur_argv = argv,
  };

  for (ParamDesc const& param : _cmd._params) {
    if (st.at_end()) {
      return fmt::format("Missing required argument '{}'", param._name);
    }
    auto parse_result = parse_arg(st, param);
    if (parse_result) {
      return parse_result;
    }
  }

  while (st._curs != nullptr) {
    switch (parse_mode) {
      case Header: {
        if (st._curs[0] == '-' && st._curs[1] == '-') {
          st._curs += 2;
          parse_mode = Longopt;
        } else if (st._curs[0] == '-') {
          st._curs++;
          parse_mode = Shortopt;
        } else {
          return fmt::format("Too many required arguments provided (at '{}')", st._curs);
        }
        break;
      }

      case Longopt: {
        char* val_split = strchr(st._curs, '=');
        std::string_view opt_name;
        if (val_split == nullptr) {
          opt_name = st._curs;
          st.nxt_arg();
        } else {
          opt_name = std::string_view(st._curs, val_split - st._curs);
          st._curs = val_split + 1;
        }

        OptionDesc const* desc = _cmd.find_opt(opt_name);
        if (desc == nullptr) {
          return fmt::format("Invalid or unrecognized option name '{}'", opt_name);
        }
        auto fail_reason = parse_opt(st, *desc);
        if (fail_reason) {
          return fmt::format("Failed to parse option '{}', Reason: {}", desc->_opt, *fail_reason);
        }
        break;
      }

      case Shortopt: {
        OptionDesc const* desc = _cmd.find_opt(*st._curs);
        if (desc == nullptr) {
          return fmt::format("Invalid or unrecognized short option name '{}'", *st._curs);
        }

        st._curs++;
        if (*st._curs == '\0') {
          st.nxt_arg();
        }
        auto fail_reason = parse_opt(st, *desc);
        if (fail_reason) {
          return fmt::format("Failed to parse option '{}', Reason: {}", desc->_shortopt, *fail_reason);
        }
        break;
      }
    }
  }

  return std::nullopt;
}

CmdParamVariant const& CommandParamList::param(size_t index) const {
  if (index > _parsed_args.size()) {
    return _invalid;
  }
  return _parsed_args[index];
}

CmdParamVariant const& CommandParamList::option(std::string_view opt) const {
  auto parsed_it =
    std::find_if(_parsed_opts.begin(), _parsed_opts.end(), [opt](auto const& po) { return opt == std::get<1>(po); });
  if (parsed_it != _parsed_opts.end()) {
    return std::get<2>(*parsed_it);
  }

  OptionDesc const* desc = _cmd.find_opt(opt);
  if (desc != nullptr) {
    return desc->_default;
  }

  return _invalid;
}

CmdParamVariant const& CommandParamList::option(char shortopt) const {
  auto parsed_it = std::find_if(
    _parsed_opts.begin(), _parsed_opts.end(), [shortopt](auto const& po) { return shortopt == std::get<0>(po); });
  if (parsed_it != _parsed_opts.end()) {
    return std::get<2>(*parsed_it);
  }

  OptionDesc const* desc = _cmd.find_opt(shortopt);
  if (desc != nullptr) {
    return desc->_default;
  }

  return _invalid;
}

int exec_command(std::vector<LaunchCommand> const& cmd_list, int argc, char** argv) {
  if (argc == 0) {
    print_usage(kDefaultProgName, cmd_list);
    return 1;
  }
  if (argc == 1) {
    print_usage(argv[0], cmd_list);
    return 1;
  }
  char const* cmdname = argv[1];
  if (strncmp(cmdname, "-h", 3) == 0 || strncmp(cmdname, "--help", 7) == 0) {
    print_help(argv[0], cmd_list);
    return 0;
  }

  for (LaunchCommand const& cmd : cmd_list) {
    if (cmd._name == cmdname) {
      for (int i = 2; i < argc; i++) {
        if (strncmp(argv[i], "-h", 3) == 0 || strncmp(argv[i], "--help", 7) == 0) {
          std::cout << fmt::format("Usage for command '{}'\n", cmd._name);
          print_cmd_help(argv[0], cmd, std::cout);
          return 0;
        }
      }

      if (argc - 2 >= 0) {
        CommandParamList cpl(cmd);
        auto fail_reason = cpl.parse_argv(argv + 2);
        if (fail_reason) {
          std::cerr << fmt::format("Error: {}\nRun '{} {} --help' for more detailed information on command\n",
            *fail_reason,
            argv[0],
            cmd._name);
          return 1;
        }

        return cmd.command_fn(cpl);
      }
    }
  }

  std::cerr << fmt::format("Unknown command '{}'\n", cmdname);
  print_usage(argv[0], cmd_list);
  return 1;
}
}  // namespace decomp
