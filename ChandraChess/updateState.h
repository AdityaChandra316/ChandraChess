#pragma once
#include <cstdint>
#include <math.h>
#include "state.h"
extern int castlingPermissionMasks[64];
void makeMove(board& inputBoard, int move);
void takeMove(board& inputBoard, int move);
void makeNullMove(board& inputBoard);
void takeNullMove(board& inputBoard);
