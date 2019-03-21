#pragma once

constexpr int GRID_CELL_SIZE = 128;

#define MAKE_GRID_CELL(x, y) (uint32_t(uint16_t(x) << 16) | uint16_t(y))
#define MAKE_GRID_INDEX(x) int(::floorf(float(x) / float(GRID_CELL_SIZE)))
#define MAKE_GRID_CELL_FROM_POS(p) (uint32_t(uint16_t(::floorf((p).x / float(GRID_CELL_SIZE))) << 16) | uint16_t(::floorf((p).y / float(GRID_CELL_SIZE))))