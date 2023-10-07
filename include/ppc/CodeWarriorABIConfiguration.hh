#pragma once

#include <cstdint>
#include <optional>

namespace decomp {
// CodeWarrior ABI configuration for the binary being decompiled
struct CWABIConfiguration {
  // If rtoc and r13 are provided to the decompiler, it can substitute in TOC references for literal
  // values
  std::optional<uint32_t> _rtoc_base;
  std::optional<uint32_t> _r13_base;

  // savegpr and restgpr can be guessed based on their structure, but it is more reliable to have a
  // set definition for them configured by the user
  std::optional<uint32_t> _savegpr_start;
  std::optional<uint32_t> _restgpr_start;
};

}  // namespace decomp
