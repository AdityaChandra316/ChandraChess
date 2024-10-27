#include <math.h>
#include <algorithm>
#include <vector>
#include <iostream>
#include "board.h"
#include "state.h"
#include "table.h"
#include "updateState.h"
#include "moveGenerator.h"
int tableEntrySize = sizeof(tableEntry);
int lowerBound = 0;
int upperBound = 1;
int exact = 2;
tableEntry* table;
uint64_t syncronisationTable[131072];
void allocateTable(int size) {
  free(table);
  numberOfEntries = size / tableEntrySize;
  table = (tableEntry*)malloc(numberOfEntries * tableEntrySize);
}
void insertToTable(board& inputBoard, int move, int depth, int score, int type) {
  tableEntry& currentTableEntry = table[inputBoard.currentKey % numberOfEntries];
  if (score >= 19936) {
    score += inputBoard.distanceToRoot;
  } else if (score <= -19936) {
    score -= inputBoard.distanceToRoot;
  }
  currentTableEntry.key = inputBoard.currentKey;
  currentTableEntry.move = move;
  currentTableEntry.depth = depth;
  currentTableEntry.score = score;
  currentTableEntry.type = type;
} 
void startingSearch(board& inputBoard, int depth) {
  if (depth <= 2) return;
  uint64_t& currentSyncronisationTableEntry = syncronisationTable[inputBoard.currentKey & 131071];
  if (currentSyncronisationTableEntry == 0ull) currentSyncronisationTableEntry = inputBoard.currentKey;
}
void endingSearch(board& inputBoard, int depth) {
  if (depth <= 2) return;
  uint64_t& currentSyncronisationTableEntry = syncronisationTable[inputBoard.currentKey & 131071];
  if (currentSyncronisationTableEntry == inputBoard.currentKey) currentSyncronisationTableEntry = 0ull;
}
bool probeTableEntry(board& inputBoard, int depth, int alpha, int beta, int& move, int& hashDepth, int& score, int& type) {
  tableEntry& probedTableEntry = table[inputBoard.currentKey % numberOfEntries];
  if (probedTableEntry.key == inputBoard.currentKey) {
    move = probedTableEntry.move;
    hashDepth = probedTableEntry.depth;
    score = probedTableEntry.score;
    if (score >= 19936) {
      score -= inputBoard.distanceToRoot;
    } else if (score <= -19936) {
      score += inputBoard.distanceToRoot;
    }
    type = probedTableEntry.type;
    if (hashDepth >= depth && (type == exact || (type == upperBound && score <= alpha) || (type == lowerBound && score >= beta))) return true;
  }
  return false;
}
