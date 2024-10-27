#include <algorithm>
#include <iostream>
#include <thread>
#include "bits.h"
#include "board.h"
#include "evaluation.h"
#include "hashKey.h"
#include "perft.h"
#include "search.h"
#include "state.h"
#include "staticExchangeEvaluation.h"
#include "table.h"
#include "moveGenerator.h"
#include "updateState.h"
void initializeMemory() {
  for (int i = 0; i < numberOfEntries; i++) {
    tableEntry& currentTableEntry = table[i];
    currentTableEntry.key = 0ull;
    currentTableEntry.move = 0;
    currentTableEntry.depth = 0;
    currentTableEntry.score = 0;
    currentTableEntry.type = 0;
  }
  for (int i = 0; i < 12; i++) {
    for (int j = 0; j < 64; j++) {
      currentBoard.history[i][j] = 0;
    }
  }
  for (int i = 0; i < 12; i++) {
    for (int j = 0; j < 64; j++) {
      for (int k = 0; k < 12; k++) {
        for (int l = 0; l < 64; l++) {
          currentBoard.onePlyHistory[i][j][k][l] = 0;
          currentBoard.twoPlyHistory[i][j][k][l] = 0;
        }
      }
    }
  }
}
int main() {
  std::cout << "Welcome to ChandraChess!" << std::endl;
  initializeMasks();
  initializeKeys();
  allocateTable(64000000);
  loadNnue();
  loadTimeManagementTable();
  initializeMemory();
  while (true) {
    std::string currentInput;
    std::getline(std::cin, currentInput);
    int currentInputSize = currentInput.size();
    int currentInputSizeMinusOne = currentInputSize - 1;
    if (currentInput == "uci") {
      std::cout << "id name ChandraChess 1.0" << std::endl;
      std::cout << "id author Aditya Chandra" << std::endl;
      std::cout << "option name Hash type spin default 64 min 1 max 2147" << std::endl;
      std::cout << "option name Clear Hash type button" << std::endl;
      std::cout << "option name Threads type spin default 1 min 1 max 8" << std::endl;
      std::cout << "uciok" << std::endl;
    }
    if (currentInput.substr(0, 8) == "position") {
      std::string fen;
      int movesPosition = currentInput.find("moves");
      if (currentInput.substr(9, 8) == "startpos") {
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
      } else {
        int end = movesPosition;
        if (end == std::string::npos) {
          end = currentInputSize;
        } else {
          end -= 1;
        }
        fen = currentInput.substr(13, end - 13);
      }
      setBoard(currentBoard, fen);
      if (movesPosition != std::string::npos) {
        int iterator = movesPosition + 6;
        std::string longAlgebraic;
        while (true) {
          int move = longAlgebraicToMove(longAlgebraic, currentBoard);
          if (iterator == currentInputSize) {
            makeHashKeyMove(currentBoard, move);
            makeMove(currentBoard, move);
            break;
          } 
          char currentCharacter = currentInput[iterator];
          if (currentCharacter == ' ') {
            makeHashKeyMove(currentBoard, move);
            makeMove(currentBoard, move);
            longAlgebraic = "";
            iterator++;
            continue;
          }
          longAlgebraic += currentCharacter;
          iterator++;
        } 
      }
    } else if (currentInput.substr(0, 2) == "go") {
      depthLimit = 64;
      timeToSearch = 2147483647;
      int depthStart = currentInput.find("depth");
      bool wasMoveTimeFound = false;
      int moveTimeStart = currentInput.find("movetime");
      int infiniteStart = currentInput.find("infinite");
      int perftStart = currentInput.find("perft");
      if (depthStart != std::string::npos) {
        depthStart += 6;
        int depthEnd = currentInput.find(' ', depthStart + 1);
        if (depthEnd == std::string::npos) depthEnd = currentInputSize;
        depthLimit = stoi(currentInput.substr(depthStart, depthEnd - depthStart));
      } else if (moveTimeStart != std::string::npos) {
        moveTimeStart += 9;
        int moveTimeEnd = currentInput.find(' ', moveTimeStart + 1);
        if (moveTimeEnd == std::string::npos) moveTimeEnd = currentInputSize;
        timeToSearch = stoi(currentInput.substr(moveTimeStart, moveTimeEnd - moveTimeStart)) - minimumTimeRemaining;
      } else if (perftStart != std::string::npos) {
        perftStart += 6;
        int perftEnd = currentInput.find(' ', perftStart + 1);
        if (perftEnd == std::string::npos) perftEnd = currentInputSize;
        int perftDepth = stoi(currentInput.substr(perftStart, perftEnd - perftStart));
        divide(perftDepth);
      } else if (infiniteStart == std::string::npos) {
        // If not given the exact move time or the depth or told to be infinite then calculate it for yourself.
        int timeIncrement = 0;
        int timeRemaining = 0;
        int movesOfOurSidePlayed = currentBoard.halfMovesDone / 2;
        double movesToGo = timeManagementTable[currentBoard.sideToPlay][movesOfOurSidePlayed];
        int movesToGoStart = currentInput.find("movestogo");
        if (movesToGo != std::string::npos) {
          movesToGoStart += 10;
          int movesToGoEnd = currentInput.find(' ', movesToGoStart + 1);
          if (movesToGoEnd == std::string::npos) movesToGoEnd = currentInputSize;
          movesToGo = std::min(movesToGo, stod(currentInput.substr(movesToGoStart, movesToGoEnd - movesToGoStart)));
        }
        std::string timeIncrementTag;
        std::string timeRemainingTag;
        if (currentBoard.sideToPlay == 0) {
          timeIncrementTag = "binc";
          timeRemainingTag = "btime";
        } else {
          timeIncrementTag = "winc";
          timeRemainingTag = "wtime";
        }
        int timeIncrementStart = currentInput.find(timeIncrementTag);
        if (timeIncrementStart != std::string::npos) {
          timeIncrementStart += 5;
          int timeIncrementEnd = currentInput.find(' ', timeIncrementStart + 1);
          if (timeIncrementEnd == std::string::npos) timeIncrementEnd = currentInputSize;
          timeIncrement = stoi(currentInput.substr(timeIncrementStart, timeIncrementEnd - timeIncrementStart));
        }
        int timeRemainingStart = currentInput.find(timeRemainingTag);
        if (timeRemainingStart != std::string::npos) {
          timeRemainingStart += 6;
          int timeRemainingEnd = currentInput.find(' ', timeRemainingStart + 1);
          if (timeRemainingEnd == std::string::npos) timeRemainingEnd = currentInputSize;
          timeRemaining = stoi(currentInput.substr(timeRemainingStart, timeRemainingEnd - timeRemainingStart));
        }
        if (timeRemaining - timeIncrement - minimumTimeRemaining < 0) {
          timeToSearch = timeRemaining - minimumTimeRemaining;
        } else {
          timeToSearch = int(std::round((double(timeRemaining) + double(timeIncrement) * (movesToGo - 1.0) - double(minimumTimeRemaining)) / movesToGo));
        }
      }
      if (perftStart == std::string::npos) {
        prepareForSearch(currentBoard);
        std::thread searcherThread(searchPosition, std::ref(currentBoard));
        searcherThread.detach();
      }
    } else if (currentInput.substr(0, 9) == "setoption") {
      if (currentInput.substr(15, 10) == "Clear Hash") {
        initializeMemory();
      } else if (currentInput.substr(15, 10) == "Hash value") {
        int hashSizeInMegabytes = stoi(currentInput.substr(26, currentInputSize - 26));
        allocateTable(hashSizeInMegabytes * 1000000);
      } else if (currentInput.substr(15, 7) == "Threads") {
        numberOfThreads = stoi(currentInput.substr(29, currentInputSize - 29));
      }
    } else if (currentInput == "ucinewgame") {
      initializeMemory();
    } else if (currentInput == "stop") {
      isInterruptedByGui = true;
    } else if (currentInput == "quit") {
      isInterruptedByGui = true;
      while (isCurrentlySearching);
      return 0;
    } else if (currentInput == "isready") {
      std::cout << "readyok" << std::endl;
    } else if (currentInput == "d") {
      printBoard(currentBoard);
    } else if (currentInput == "eval") {
      initializeAccumulators(currentBoard);
      std::cout << "Static Eval: " << (2 * currentBoard.sideToPlay - 1) * evaluate(currentBoard) << " cp" << std::endl;
    } else if (currentInput.substr(0, 3) == "see") {
      std::string longAlgebraic = currentInput.substr(4, currentInputSize - 4);
      std::cout << runStaticExchangeEvaluation(currentBoard, longAlgebraicToMove(longAlgebraic, currentBoard)) << std::endl;
    }
  }
  return 0;
}
