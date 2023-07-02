#pragma once

#include <fstream>
#include <memory>

#include "CodeWarriorABIConfiguration.hh"
#include "producers/RandomAccessData.hh"
#include "utl/ErrorOr.hh"

namespace decomp {
enum class BinaryType {
  kDOL,
  // kREL,
  // kELF,
};
struct BinaryContext {
  BinaryType _btype;
  std::unique_ptr<RandomAccessData> _ram;
  CWABIConfiguration _abi_conf;
  std::optional<uint32_t> _entrypoint;
};

ErrorOr<BinaryContext> create_from_stream(std::ifstream& data_in, BinaryType btype, bool do_abi_discovery = true);
bool is_abi_routine(BinaryContext const& ctx, uint32_t addr);
}  // namespace decomp
