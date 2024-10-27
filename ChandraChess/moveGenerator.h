#pragma once
#include <cstdint>
#include <vector>
#include "state.h"
struct moveEntry {
  int move;
  int score;
};
struct movesContainer {
  int numberOfMoves = 0;
  moveEntry moveList[256];
};
void generateMoves(board& inputBoard, movesContainer& moves, bool isQuiescentGenerator);
void orderMoves(board& inputBoard, movesContainer& moves, int hashMove);
