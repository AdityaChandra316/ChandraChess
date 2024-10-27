#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include "bits.h"
#include "board.h"
#include "moveGenerator.h"
#include "staticExchangeEvaluation.h"
#include "state.h"
#include "updateState.h"
#define THREADS 8
void pgnExtractWorker(int thread, board* inputBoards, unsigned long* fileOffsets, long double* timeManagementTable, volatile int* progress, volatile bool* finished) {
  std::ifstream inputData("data.pgn");
  std::ofstream outputData("./shards/" + std::to_string(thread) + ".txt");
  inputData.clear();
  inputData.seekg(fileOffsets[thread]);
  unsigned long nextFileOffset = fileOffsets[thread + 1];
  bool isAcceptingScore = false;
  bool isAcceptingWord = false;
  std::string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  board& inputBoard = inputBoards[thread];
  setBoard(inputBoard, startFen);
  std::string word;
  int timeManagementTableOffset = 2048 * thread;
  while (true) {
    inputData >> word;
    if (word == "1.") {
      isAcceptingWord = true;
    } else if (word == "1/2-1/2" || word == "1-0" || word == "0-1" || word == "*") {
      // Collect statistics on expected game length
      double halfMovesDoneOverTwo = double(inputBoard.halfMovesDone) / 2.0;
      int numberOfWhiteMoves = int(ceil(halfMovesDoneOverTwo));
      int numberOfBlackMoves = int(floor(halfMovesDoneOverTwo));
      for (int i = 0; i < numberOfWhiteMoves; i++) {
        int movesRemaining = numberOfWhiteMoves - i;
        int timeManagementTableIndex = timeManagementTableOffset + 1024 + 2 * i;
        timeManagementTable[timeManagementTableIndex] += 1.0l;
        timeManagementTable[timeManagementTableIndex + 1] += (long double)movesRemaining;
      }
      for (int i = 0; i < numberOfBlackMoves; i++) {
        int movesRemaining = numberOfBlackMoves - i;
        int timeManagementTableIndex = timeManagementTableOffset + 2 * i;
        timeManagementTable[timeManagementTableIndex] += 1.0l;
        timeManagementTable[timeManagementTableIndex + 1] += (long double)movesRemaining;
      }
      isAcceptingWord = false; // Don't accept words because there will not be moves for a little while
      progress[thread] = progress[thread] + 1; // Game is finished, so increment counter
      setBoard(inputBoard, startFen); // Reset the board for the next game
      if ((unsigned long)inputData.tellg() == nextFileOffset) break;
    } else if (word == "[%eval") {
      isAcceptingScore = true;
    } else if (isAcceptingScore) {
      // Filter out positions which have the sideToPlay in check.
      if (isSquareOfSideToPlayAttacked(inputBoard, msbPosition(inputBoard.bitboards[6 * inputBoard.sideToPlay]))) {
        isAcceptingScore = false;
        continue;
      }
      // Filter out positions which have obviously good captures and promotions for the sideToPlay.
      movesContainer moves;
      generateMoves(inputBoard, moves, true);
      bool isThereCaptureOrPromotion = false;
      for (int i = 0; i < moves.numberOfMoves; i++) {
        if (runStaticExchangeEvaluation(inputBoard, moves.moveList[i].move) > 0) {
          isThereCaptureOrPromotion = true;
          break;
        }
      }
      if (isThereCaptureOrPromotion) {
        isAcceptingScore = false;
        continue;
      }
      // Calculate score.
      std::string scoreStripped = word.substr(0, word.size() - 1);
      double score;
      if (scoreStripped[0] == '#') {
        if (scoreStripped[1] == '-') {
          double mateIn = stod(scoreStripped.substr(2, scoreStripped.size() - 2));
          score = -32000.0 + mateIn;
        } else {
          double mateIn = stod(scoreStripped.substr(1, scoreStripped.size() - 1));
          score = 32000.0 - mateIn;
        }
      } else {
        score = stod(scoreStripped);
      }
      score = 1.0 / (1.0 + std::exp(-score / 3.2768)); // 2.56 score is 1 pawn for Stockfish, and we multiply that by 1.28 for quantization
      score = 2.0 * score - 1.0;
      score *= (2.0 * inputBoard.sideToPlay - 1.0);
      score = (score + 1.0) / 2.0;
      // Write data.
      std::string fen;
      boardToFen(inputBoard, fen);
      outputData << fen << ',' << std::to_string(score) << std::endl;
      isAcceptingScore = false;
    } else if (isAcceptingWord && isalpha(word[0])) {
      std::string algebraicMove = "";
      // Remove annotations from algebraicMove.
      for (int i = 0; i < word.size(); i++) {
        char wordToken = word[i];
        if (wordToken != '!' && wordToken != '?') algebraicMove += wordToken;
      }
      makeMove(inputBoard, algebraicToMove(algebraicMove, inputBoard));
    }
  }
  inputData.close();
  outputData.close();
  finished[thread] = true;
}
int main() {
  initializeMasks();
  std::ifstream inputData("data.pgn");
  std::ofstream outputTimeManagementTable("time_management_table.txt");
  std::string word;
  int gamesDone = 0;
  long double* timeManagementTable = (long double*)calloc(2048 * THREADS, sizeof(long double));
  int numberOfGames = 0;
  while (inputData >> word) {
    if (word == "1/2-1/2" || word == "1-0" || word == "0-1" || word == "*") numberOfGames++;
  }
  std::cout << "Calculated Number of Games" << std::endl;
  int threadSizes[THREADS];
  int threadOffsets[THREADS + 1];
  int remainder = numberOfGames % THREADS;
  int quotient = numberOfGames / THREADS;
  for (int i = 0; i < THREADS; i++) threadSizes[i] = quotient;
  for (int i = 0; i < remainder; i++) threadSizes[i] += 1;
  threadOffsets[0] = 0;
  int sum = 0;
  for (int i = 1; i <= THREADS; i++) {
    sum += threadSizes[i - 1];
    threadOffsets[i] = sum;
  }
  inputData.clear();
  inputData.seekg(0);
  numberOfGames = 0;
  unsigned long fileOffsets[THREADS + 1];
  fileOffsets[0] = 0ull;
  int currentThread = 1;
  while (inputData >> word) {
    if (word == "1/2-1/2" || word == "1-0" || word == "0-1" || word == "*") {
      numberOfGames++;
      if (numberOfGames == threadOffsets[currentThread]) {
        fileOffsets[currentThread] = (unsigned long)inputData.tellg();
        currentThread++;
      }
    }
  }
  std::cout << "Set File Offsets" << std::endl;
  std::thread workerThreads[THREADS];
  volatile bool finished[THREADS];
  volatile int progress[THREADS];
  for (int i = 0; i < THREADS; i++) {
    finished[i] = false;
    progress[i] = 0;
  }
  board* inputBoards = (board*)malloc(THREADS * sizeof(board));
  for (int i = 0; i < THREADS; i++) workerThreads[i] = std::thread(pgnExtractWorker, i, inputBoards, fileOffsets, timeManagementTable, progress, finished);
  int lastProgress = 0;
  int progressReportInterval = 10000;
  while (true) {
    int totalProgress = 0;
    for (int i = 0; i < THREADS; i++) totalProgress += progress[i];
    if (totalProgress - lastProgress > progressReportInterval) {
      std::cout << "Extracted: " << totalProgress << std::endl;
      lastProgress = totalProgress;
    }
    bool shouldBreak = true;
    for (int i = 0; i < THREADS; i++) {
      if (!finished[i]) {
        shouldBreak = false;
        break;
      }
    }
    if (shouldBreak) break;
  }
  for (int i = 0; i < THREADS; i++) {
    if (workerThreads[i].joinable()) {
      workerThreads[i].join();
    }
  }
  inputData.close(); 
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 512; j++) {
      long double denominator = 0.0l;
      long double numerator = 0.0l;
      for (int k = 0; k < THREADS; k++) {
        int timeManagementTableIndex = 2048 * k + 1024 * i + 2 * j;
        denominator += timeManagementTable[timeManagementTableIndex];
        numerator += timeManagementTable[timeManagementTableIndex + 1];
      }
      outputTimeManagementTable << (denominator == 0.0l ? 1.0l : numerator / denominator) << " ";
    }
  }
  return 0; 
}
