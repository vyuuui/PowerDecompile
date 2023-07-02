#pragma once

#include <string_view>

#include "producers/DolData.hh"

namespace decomp {
// Pattern language
//   pat  := <byte> | <byte> ' ' <pat>
//   byte := <hex> <hex> | '??'
//   hex  := [0-9A-Fa-f]
std::optional<uint32_t> pattern_scan_code(DolData const& data, std::string_view pattern);
}  // namespace decomp
