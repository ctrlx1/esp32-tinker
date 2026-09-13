#pragma once

#include "types.h"

namespace MazeHero {

class Maze {
public:
  void generate(unsigned int minWidth, unsigned int maxWidth,
                unsigned int minHeight, unsigned int maxHeight);

  uint8_t width() const { return mazeWidth; }
  uint8_t height() const { return mazeHeight; }
  Coord start() const { return startCoord; }
  Coord finish() const { return finishCoord; }

  bool contains(Coord coord) const;
  bool hasWall(Coord coord, Direction dir) const;
  bool neighbor(Coord coord, Direction dir, Coord &out) const;
  bool canMove(Coord coord, Direction dir, Coord &out) const;
  uint16_t index(Coord coord) const;

private:
  struct Cell {
    uint8_t walls = 0x0F;
  };

  Cell cells[MAX_MAZE_CELLS];
  bool visited[MAX_MAZE_CELLS];
  Coord stack[MAX_MAZE_CELLS];
  Coord queue[MAX_MAZE_CELLS];
  int16_t distances[MAX_MAZE_CELLS];

  uint8_t mazeWidth = MIN_MAZE_DIMENSION;
  uint8_t mazeHeight = MIN_MAZE_DIMENSION;
  Coord startCoord;
  Coord finishCoord;

  void resetCells();
  void carveMaze();
  void clearWall(Coord coord, Direction dir);
  Coord randomEdgeCell() const;
  bool isEdgeCell(Coord coord) const;
  Direction outwardEdgeDirection(Coord coord) const;
  void openEdge(Coord coord);
  void placeStartAndFinish();
};

} // namespace MazeHero
