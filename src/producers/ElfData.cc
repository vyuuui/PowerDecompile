#include "producers/ElfData.hh"

#include <cstring>
#include <istream>

#include "utl/elf.h"

namespace decomp {
namespace {
template <typename T>
T byteswap(T val) {
  T out;
  for (size_t i = 0; i < sizeof(T); i++) {
    reinterpret_cast<uint8_t*>(&out)[sizeof(T) - i - 1] = reinterpret_cast<uint8_t*>(&val)[i];
  }
  return out;
}
}  // namespace

bool ElfData::load_from(std::istream& source) {
  Elf32_Ehdr hdr;
  memset(&hdr, 0, sizeof(Elf32_Ehdr));

  source.read(reinterpret_cast<char*>(&hdr), sizeof(Elf32_Ehdr));
  if (hdr.e_ident[EI_MAG0] != ELFMAG0 || hdr.e_ident[EI_MAG1] != ELFMAG1 || hdr.e_ident[EI_MAG2] != ELFMAG2 ||
      hdr.e_ident[EI_MAG3] != ELFMAG3) {
    return false;
  }
  if (hdr.e_ident[EI_DATA] != ELFDATA2MSB) {
    return false;
  }

  hdr.e_machine = byteswap(hdr.e_machine);
  hdr.e_entry = byteswap(hdr.e_entry);
  hdr.e_shoff = byteswap(hdr.e_shoff);
  hdr.e_shnum = byteswap(hdr.e_shnum);

  if (hdr.e_machine != EM_PPC) {
    return false;
  }

  source.seekg(hdr.e_shoff);

  for (Elf32_Half i = 0; i < hdr.e_shnum; i++) {
    Elf32_Shdr shdr;
    memset(&shdr, 0, sizeof(Elf32_Shdr));

    source.read(reinterpret_cast<char*>(&shdr), sizeof(Elf32_Shdr));

    shdr.sh_type = byteswap(shdr.sh_type);
    shdr.sh_addr = byteswap(shdr.sh_addr);
    shdr.sh_size = byteswap(shdr.sh_size);
    shdr.sh_offset = byteswap(shdr.sh_offset);
    shdr.sh_flags = byteswap(shdr.sh_flags);

    if (shdr.sh_type == SHT_PROGBITS && shdr.sh_addr != 0) {
      auto save_pos = source.tellg();
      std::vector<uint8_t> sect_data;
      sect_data.resize(shdr.sh_size);
      source.seekg(shdr.sh_offset);
      source.read(reinterpret_cast<char*>(sect_data.data()), sect_data.size());
      add_section(shdr.sh_addr, std::move(sect_data));

      if (shdr.sh_flags & SHF_EXECINSTR) {
        _text_sections.emplace_back(shdr.sh_offset, shdr.sh_addr, shdr.sh_size);
      } else if (shdr.sh_flags & SHF_ALLOC) {
        _data_sections.emplace_back(shdr.sh_offset, shdr.sh_addr, shdr.sh_size);
      }
      source.seekg(save_pos);
    }
  }

  _entrypoint = hdr.e_entry;

  return true;
}
}  // namespace decomp
