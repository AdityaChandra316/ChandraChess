#include <chrono>
#include <vector>
#include <iostream>
#include "perft.h"
#include "hashKey.h"
#include "moveGenerator.h"
#include "state.h"
#include "updateState.h"
#include "board.h"
#include "bits.h"
unsigned long long perft(int depth) {
  if (depth == 0) return 1ull;
  movesContainer moves;
  generateMoves(currentBoard, moves, false);
  if (depth == 1) return (unsigned long long)moves.numberOfMoves;
  unsigned long long result = 0ull;
  for (int i = 0; i < moves.numberOfMoves; i++) {
    int move = moves.moveList[i].move;          
    makeHashKeyMove(currentBoard, move);
    makeMove(currentBoard, move);
    result += perft(depth - 1);
    takeHashKeyMove(currentBoard);
    takeMove(currentBoard, move);
  }
  return result;
}
void divide(int depth) {
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
  std::cout << "Perft to depth " << depth << ":" << "\n";
  unsigned long long result = 0ull;
  movesContainer moves;
  generateMoves(currentBoard, moves, false);
  for (int i = 0; i < moves.numberOfMoves; i++) {
    int move = moves.moveList[i].move;      
    makeHashKeyMove(currentBoard, move);
    makeMove(currentBoard, move);
    unsigned long long rootResult = perft(depth - 1);
    std::string longAlgebraicMove;
    moveToLongAlgebraic(move, longAlgebraicMove);
    std::cout << longAlgebraicMove << ": " << rootResult << std::endl;
    takeHashKeyMove(currentBoard);
    takeMove(currentBoard, move);
    result += rootResult;
  }
  std::cout << "Total leaf nodes: " << result << "\n";
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  long double timeElapsed = (long double)std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
  std::cout << (((long double)result / (timeElapsed / (long double)(1000))) / (long double)(1000)) << " kN/s" << "\n\n";
}
