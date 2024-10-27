#pragma once
#include <cstdint>
#include <math.h>
#include <vector>
#include <string>
#define ACCUMULATOR_SIZE 512
struct historyEntry {
  int castlingPermission;
  int pieceCapturedOnNextMove;
  int halfMoveClock;
};
struct historyEntryForNull {
  int enPassantSquare;
  int piece;
  int to;
};
struct accumulatorsHistoryEntry {
  int whiteAccumulator[ACCUMULATOR_SIZE];
  int blackAccumulator[ACCUMULATOR_SIZE];
};
struct board {
  int nodes;
  bool isSearchStopped;
  uint64_t bitboards[15];
  int mailbox[64];
  int sideToPlay;
  int castlingPermission;
  int enPassantSquare;
  int whiteAccumulator[ACCUMULATOR_SIZE];
  int blackAccumulator[ACCUMULATOR_SIZE];
  int halfMoveClock;
  int halfMovesDone;
  uint64_t currentKey;
  historyEntry histories[1024];
  accumulatorsHistoryEntry accumulatorsHistory[64];
  historyEntryForNull historiesForNull[1024];
  uint64_t hashKeyHistory[1024];
  int numberOfHistory;
  int numberOfAccumulatorsHistory;
  int numberOfHistoryForNull;
  int numberOfHashKeyHistory;
  int distanceToRoot;
  int killersTable[64][2];
  int history[12][64];
  int onePlyHistory[12][64][12][64];
  int twoPlyHistory[12][64][12][64];
};
extern board currentBoard;
extern int numberOfEntries;
