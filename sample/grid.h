#pragma once

constexpr int GRID_CELL_SIZE = 128;
constexpr int SMALL_GRID_CELL_SIZE = 32;

#define MAKE_GRID_CELL(x, y) (uint32_t(uint16_t(x) << 16) | uint16_t(y))
#define MAKE_GRID_INDEX(x, sz) int(::floorf(float(x) / float(sz)))
#define MAKE_GRID_CELL_FROM_POS(p, sz) (uint32_t(uint16_t(::floorf((p).x / float(sz))) << 16) | uint16_t(::floorf((p).y / float(sz))))