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
void loadTimeManagementTable();
void searchPosition(board& inputBoard);
void prepareForSearch(board& inputBoard);
extern volatile bool isCurrentlySearching;
void printHistory();
