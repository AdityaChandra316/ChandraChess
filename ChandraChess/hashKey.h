#pragma once
#include <cstdint>
#include <math.h>
#include "state.h"
extern uint64_t positionKeys[13][64];
extern uint64_t sideToPlayKeys[2];
extern uint64_t castlingPermissionsKeys[16];
extern uint64_t enPassantSquareKeys[64];
void initializeKeys();
uint64_t getKey(board& inputBoard);
void makeHashKeyMove(board& inputBoard, int move);
void takeHashKeyMove(board& inputBoard);
