#pragma once
#include <cstdint>
#include <string>
#include "state.h"
extern char pieces[12];
extern uint64_t blockerMasks[2];
void setBoard(board& inputBoard, std::string& fen);
uint64_t msbRayAttacks(board& inputBoard, int square, int ray);
uint64_t lsbRayAttacks(board& inputBoard, int square, int ray);
uint64_t generalRayAttacks(board& inputBoard, int square, int ray);
bool isSquareOfSideToPlayAttacked(board& inputBoard, int square);
int longAlgebraicToMove(std::string& longAlgebraic, board& inputBoard);
void moveToLongAlgebraic(int move, std::string& longAlgebraicMove);
void printBoard(board& inputBoard);
bool isRepetition(board& inputBoard);
int algebraicToMove(std::string& algebraic, board& inputBoard);
void boardToFen(board& inputBoard, std::string& fen);
bool isMoveCausingCheck(board& inputBoard, int move);
