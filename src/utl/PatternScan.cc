#include "utl/PatternScan.hh"

#include <cctype>
#include <vector>

#include "producers/DolData.hh"
#include "producers/RandomAccessData.hh"
#include "producers/SectionedData.hh"

namespace decomp {
namespace {
using CompiledPattern = std::vector<std::optional<uint8_t>>;

CompiledPattern compile_pattern(std::string_view pattern) {
  constexpr auto hex_nib = [](char c) -> uint8_t {
    if (c >= '0' && c <= '9') {
      return c - '0';
    } else if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    } else {
      return c - 'A' + 10;
    }
  };

  CompiledPattern ret;
  for (size_t i = 0; i + 1 < pattern.size(); i += 3) {
    if (pattern[i] == '?' && pattern[i + 1] == '?') {
      ret.push_back(std::nullopt);
    } else if (std::isxdigit(pattern[i]) && std::isxdigit(pattern[i + 1])) {
      ret.push_back((hex_nib(pattern[i]) << 4) | hex_nib(pattern[i + 1]));
    }
  }

  return ret;
}

std::optional<uint32_t> pattern_scan_linear(std::vector<uint8_t> const& raw_data, CompiledPattern const& cpat) {
  for (size_t i = 0; (i + cpat.size()) < raw_data.size(); i++) {
    bool found = true;
    for (size_t j = 0; j < cpat.size(); j++) {
      if (!cpat[j]) {
        continue;
      }

      if (*cpat[j] != raw_data[i + j]) {
        found = false;
        break;
      }
    }

    if (found) {
      return static_cast<uint32_t>(i);
    }
  }

  return std::nullopt;
}
}  // namespace

std::optional<uint32_t> pattern_scan_code(DolData const& data, std::string_view pattern) {
  CompiledPattern cpat = compile_pattern(pattern);

  for (decomp::DolSection const& dol_sect : data.text_section_headers()) {
    decomp::Section const* sect = data.section_for_vaddr(dol_sect._vaddr);
    if (sect == nullptr) {
      continue;
    }

    std::optional<uint32_t> found_addr = pattern_scan_linear(sect->_data, cpat);
    if (found_addr) {
      return *found_addr + sect->_base;
    }
  }

  return std::nullopt;
}
}  // namespace decomp
