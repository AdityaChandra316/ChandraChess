#pragma once
#include <string>
#include <vector>
#include "state.h"
extern double timeManagementTable[2][512];
extern int numberOfThreads;
extern int timeToSearch;
extern int depthLimit;
extern int minimumTimeRemaining;
extern volatile bool isInterruptedByGui;
struct principalVariationContainer {
  int numberOfMoves = 0;
  int moveList[64];
};
void killAllThreads();
void setupThreadData();
void loadTimeManagementTable();
void searchPosition(board& inputBoard);
void prepareForSearch(board& inputBoard);
extern volatile bool isCurrentlySearching;
void printHistory();
