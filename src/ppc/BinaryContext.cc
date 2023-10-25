#include "ppc/BinaryContext.hh"

#include <memory>

#include "producers/DolData.hh"
#include "utl/ErrorOr.hh"
#include "utl/PatternScan.hh"

namespace decomp::ppc {
namespace {
constexpr std::string_view kSaveGprPattern =
  "91 cb ff b8 91 eb ff bc 92 0b ff c0 92 2b ff c4 92 4b ff c8 92 6b ff cc 92 8b ff d0 92 ab ff d4 92 cb ff d8 92 eb "
  "ff dc 93 0b ff e0 93 2b ff e4 93 4b ff e8 93 6b ff ec 93 8b ff f0 93 ab ff f4 93 cb ff f8 93 eb ff fc 4e 80 00 20";
constexpr std::string_view kRestGprPattern =
  "81 cb ff b8 81 eb ff bc 82 0b ff c0 82 2b ff c4 82 4b ff c8 82 6b ff cc 82 8b ff d0 82 ab ff d4 82 cb ff d8 82 eb "
  "ff dc 83 0b ff e0 83 2b ff e4 83 4b ff e8 83 6b ff ec 83 8b ff f0 83 ab ff f4 83 cb ff f8 83 eb ff fc 4e 80 00 20";

ErrorOr<BinaryContext> load_dol(std::ifstream& data_in, bool do_abi_discovery) {
  BinaryContext ret;
  ret._btype = BinaryType::kDOL;

  std::unique_ptr<DolData> ram = std::make_unique<DolData>();
  if (!ram->load_from(data_in)) {
    return "Failed to parse DOL file, invalid format";
  }

  ret._entrypoint = ram->entrypoint();
  if (do_abi_discovery) {
    ret._abi_conf._savegpr_start = pattern_scan_code(*ram, kSaveGprPattern);
    ret._abi_conf._restgpr_start = pattern_scan_code(*ram, kRestGprPattern);
    // TODO: r2/r13
  }

  ret._ram = std::unique_ptr<RandomAccessData>(ram.release());

  return ret;
}
}  // namespace

ErrorOr<BinaryContext> create_from_stream(std::ifstream& data_in, BinaryType btype, bool do_abi_discovery) {
  switch (btype) {
    case BinaryType::kDOL:
      return load_dol(data_in, do_abi_discovery);
    default:
      return "Invalid binary type";
  }
}

BinaryContext create_raw(uint32_t base, uint32_t entrypoint, char const* data, size_t len) {
  BinaryContext ret;

  ret._btype = BinaryType::kRaw;
  ret._entrypoint = entrypoint;

  std::unique_ptr<SectionedData> ram = std::make_unique<SectionedData>();
  ram->add_section(base, std::string_view(data, len));
  ret._ram = std::unique_ptr<RandomAccessData>(ram.release());

  return ret;
}

bool is_abi_routine(BinaryContext const& ctx, uint32_t addr) {
  if (ctx._abi_conf._savegpr_start) {
    if (addr >= *ctx._abi_conf._savegpr_start && addr < *ctx._abi_conf._savegpr_start + 0x4c) {
      return true;
    }
  }
  if (ctx._abi_conf._restgpr_start) {
    if (addr >= *ctx._abi_conf._restgpr_start && addr < *ctx._abi_conf._restgpr_start + 0x4c) {
      return true;
    }
  }

  return false;
}
}  // namespace decomp::ppc
