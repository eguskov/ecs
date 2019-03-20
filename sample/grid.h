#pragma once

constexpr int GRID_CELL_COUNT = 256;
constexpr int GRID_CELL_HALF_COUNT = 128;
constexpr int GRID_CELL_SIZE = 128;

#define MAKE_GRID_CELL(x, y) ((int(x) + GRID_CELL_HALF_COUNT) + (int(y) + GRID_CELL_HALF_COUNT - 1) * GRID_CELL_COUNT)
#define MAKE_GRID_INDEX(x) int(::floorf(float(x) / float(GRID_CELL_SIZE)))
#define MAKE_GRID_CELL_FROM_POS(p) ((int(::floorf((p).x / float(GRID_CELL_SIZE))) + GRID_CELL_HALF_COUNT) + (int(::floorf((p).y / float(GRID_CELL_SIZE))) + GRID_CELL_HALF_COUNT - 1) * GRID_CELL_COUNT)

#define UNPACK_GRID_CELL_X(cell) (((cell) % GRID_CELL_COUNT) - GRID_CELL_HALF_COUNT)
#define UNPACK_GRID_CELL_Y(cell) (((cell) / GRID_CELL_COUNT) - GRID_CELL_HALF_COUNT + 1)