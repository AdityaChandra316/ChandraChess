#pragma once
#include <cstdint>
#include <math.h>
#include <string>
extern uint64_t bits[64];
extern uint64_t kingAttacks[64];
extern uint64_t rayAttacks[8][64];
extern uint64_t knightAttacks[64];
extern uint64_t pawnAttacks[2][64];
extern uint64_t files[8];
extern uint64_t ranks[8];
extern int oneDimensionalToTwoDimensional[64][2];
extern int fromToToRays[64][64];
extern int oppositeRays[8];
extern int castlingPermissions[4];
void printBitboard(uint64_t bitboard);
void initializeMasks();
int msbPosition(uint64_t x);
int lsbPosition(uint64_t x);
