#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "SubroutineGraph.hh"

namespace decomp::vis {

struct Block {
  Block(uint32_t address, uint32_t inst_count) : _address(address), _inst_count(inst_count) {}
  uint32_t _address;
  uint32_t _inst_count;
};


struct BlockCell {
  BlockCell(BasicBlock* basic_block, uint32_t row, uint32_t column) : _basic_block(basic_block), _row(row), _column(column) {}
  BasicBlock* _basic_block;
  uint32_t _row;
  uint32_t _column;
};

struct Link {
  Link(size_t from_row, size_t from_block, size_t to_row, size_t to_block)
    : _from_row(from_row), _from_block(from_block), _to_row(to_row), _to_block(to_block) {}
  uint32_t _from_row, _from_block;
  uint32_t _to_row, _to_block;
};

struct VerticalGraph {
  std::vector<std::vector<Block>> _rows;
  std::vector<Link> _edges;
};

VerticalGraph visualize_vertical(SubroutineGraph const& graph);

}  // namespace decomp::vis
