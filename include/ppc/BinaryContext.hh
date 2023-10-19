#pragma once

#include <fstream>
#include <memory>

#include "ppc/CodeWarriorABIConfiguration.hh"
#include "producers/RandomAccessData.hh"
#include "utl/ErrorOr.hh"

namespace decomp::ppc {
enum class BinaryType {
  kRaw,
  kDOL,
  // kREL,
  // kELF,
  // kRSO,
};
struct BinaryContext {
  BinaryType _btype;
  std::unique_ptr<RandomAccessData> _ram;
  CWABIConfiguration _abi_conf;
  std::optional<uint32_t> _entrypoint;
};

ErrorOr<BinaryContext> create_from_stream(std::ifstream& data_in, BinaryType btype, bool do_abi_discovery = true);
BinaryContext create_raw(uint32_t, uint32_t, char const*, size_t);
template <size_t N>
BinaryContext create_raw(uint32_t base, uint32_t entrypoint, char const (&data)[N]) {
  return create_raw(base, entrypoint, data, N);
}
bool is_abi_routine(BinaryContext const& ctx, uint32_t addr);
}  // namespace decomp::ppc
