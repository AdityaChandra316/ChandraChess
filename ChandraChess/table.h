#pragma once
#include <math.h>
#include <vector>
#include "state.h"
struct tableEntry {
  uint64_t key;
  int move;
  int depth;
  int score;
  int type;
};
extern tableEntry* table;
extern uint64_t syncronisationTable[131072];
extern int lowerBound;
extern int upperBound;
extern int exact;
extern int tableSize;
void allocateTable(int size);
void insertToTable(board& inputBoard, int move, int depth, int score, int type);
void startingSearch(board& inputBoard, int depth);
void endingSearch(board& inputBoard, int depth);
bool probeTableEntry(board& inputBoard, int depth, int alpha, int beta, int& move, int& hashDepth, int& score, int& type);
