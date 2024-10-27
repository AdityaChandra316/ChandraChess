#pragma once
#include "state.h"
extern int staticPieceValues[6];
int runStaticExchangeEvaluation(board& inputBoard, int move);
int staticExchangeEvaluation(board& inputBoard, int square, uint64_t attackers, int lastPieceToMove);
