#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "bits.h"
#include "board.h"
#include "evaluation.h"
#include "hashKey.h"
#include "moveGenerator.h"
#include "search.h"
#include "state.h"
#include "staticExchangeEvaluation.h"
#include "table.h"
#include "updateState.h"
double timeManagementTable[2][512];
std::chrono::steady_clock::time_point startTime;
int boardSize = sizeof(board);
int numberOfThreads = 1;
int depthLimit = 64;
int timeToSearch;
int minimumTimeRemaining = 0;
volatile bool isInterruptedByGui;
volatile bool isCurrentlySearching;
void loadTimeManagementTable() {
  std::ifstream input("./time_management_table.txt");
  std::string parameter;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 512; j++) {
      input >> parameter;
      timeManagementTable[i][j] = std::stod(parameter);
    }
  }
}
void checkUp(board& inputBoard) {
  std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
  int timeElapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
  if (timeElapsed > timeToSearch || isInterruptedByGui) inputBoard.isSearchStopped = true;
}
int quiescence(board& inputBoard, int alpha, int beta) {
  if ((inputBoard.nodes & 2047) == 0) checkUp(inputBoard);
  int standingPat = evaluate(inputBoard);
  if (inputBoard.distanceToRoot == 64) return standingPat;
  if (standingPat >= beta) return beta;
  if (alpha < standingPat) alpha = standingPat;
  movesContainer moves;
  generateMoves(inputBoard, moves, true);
  orderMoves(inputBoard, moves, 0);
  for (int i = 0; i < moves.numberOfMoves; i++) {
    moveEntry& moveStructure = moves.moveList[i];
    int move = moveStructure.move;
    int moveOrderingScore = moveStructure.score;
    // Score for losing capture.
    // Prune these moves.
    if (moveOrderingScore < 1525) break;
    makeAccumulatorMove(inputBoard, move);
    makeHashKeyMove(inputBoard, move);
    makeMove(inputBoard, move);
    inputBoard.nodes++;
    int score = -quiescence(inputBoard, -beta, -alpha);
    takeAccumulatorMove(inputBoard);
    takeHashKeyMove(inputBoard);
    takeMove(inputBoard, move);
    if (inputBoard.isSearchStopped) return 0;
    if (score > alpha) {
      alpha = score;
      if (score >= beta) return beta;
    }
  }
  return alpha;
}
// isSideToPlayInCheck is used in some search heuristics
int negamax(board& inputBoard, int depth, int alpha, int beta, std::vector<int>& principalVariation, bool isSideToPlayInCheck) {
  if ((inputBoard.nodes & 2047) == 0) checkUp(inputBoard);
  if (isSideToPlayInCheck) depth++;
  movesContainer moves;
  generateMoves(inputBoard, moves, false);
  if (isSideToPlayInCheck && moves.numberOfMoves == 0) return -20000 + inputBoard.distanceToRoot;
  if (moves.numberOfMoves == 0 || std::popcount(inputBoard.bitboards[14]) == 2 || inputBoard.halfMoveClock == 100 || isRepetition(inputBoard)) return 0;
  bool isPrincipleVariationNode = beta - alpha > 1;
  int hashMove = 0;
  int hashDepth, hashScore, type;
  bool isRetuningHashScore = probeTableEntry(inputBoard, depth, alpha, beta, hashMove, hashDepth, hashScore, type);
  if (!isPrincipleVariationNode && isRetuningHashScore) return hashScore;
  if (depth >= 6 && isPrincipleVariationNode && hashMove == 0) depth--; // Reduce search
  if (depth == 0 || inputBoard.distanceToRoot == 64) return quiescence(inputBoard, alpha, beta);
  bool isPositionNoisy = isPrincipleVariationNode || isSideToPlayInCheck;
  int futilityMargin;
  if (!isPositionNoisy) {
    int staticEvaluation = evaluate(inputBoard);
    int evaluationMargin = 100 * depth;
    if (staticEvaluation - evaluationMargin >= beta) return beta;
    futilityMargin = staticEvaluation + evaluationMargin;
  }
  std::vector<int> currentPrincipalVariation;
  int ourOffset = 6 * inputBoard.sideToPlay;
  if (!isPositionNoisy && depth >= 4 && (
    inputBoard.bitboards[12 + inputBoard.sideToPlay] ^ 
    inputBoard.bitboards[ourOffset] ^ 
    inputBoard.bitboards[ourOffset + 5]
  ) != 0ull) {
    makeNullMove(inputBoard);
    inputBoard.nodes++;
    int score = -negamax(inputBoard, depth - 3, -beta, -beta + 1, currentPrincipalVariation, false);
    takeNullMove(inputBoard);
    if (inputBoard.isSearchStopped) return 0;
    if (score >= beta) return beta;
  }
  int bestMove = 0;
  int oldAlpha = alpha;
  orderMoves(inputBoard, moves, hashMove);
  std::vector<int> deferredMoveIndices;
  int depthSquared = depth * depth;
  int increment = std::min(32 * depthSquared, 18311); // For history heuristic
  int staticExchangeEvaluationLossMargin = -15 * depthSquared;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < moves.numberOfMoves; j++) {
      if (i == 1) {
        bool isDeferringMove = std::binary_search(deferredMoveIndices.begin(), deferredMoveIndices.end(), j);
        if (!isDeferringMove) continue;
      }
      moveEntry& moveStructure = moves.moveList[j];
      int move = moveStructure.move;
      int moveOrderingScore = moveStructure.score;
      int from = move & 0x3f;
      int to = (move & 0xfc0) >> 6;
      int type = (move & 0xc000) >> 14;
      makeHashKeyMove(inputBoard, move);
      bool isDeferredBySyncronisation = i == 0 && j != 0 && depth >= 3 && syncronisationTable[inputBoard.currentKey & 131071] == inputBoard.currentKey;
      if (isDeferredBySyncronisation) {
        deferredMoveIndices.push_back(j);
        takeHashKeyMove(inputBoard);
        continue;
      }
      bool isTacticalMove = type == 1 || type == 2 || inputBoard.mailbox[to] != 12;
      bool isMoveResultingInCheck = isMoveCausingCheck(inputBoard, move);
      bool isMoveNoisy = isPositionNoisy || moveOrderingScore >= 38148 || isMoveResultingInCheck;
      if (!isMoveNoisy) {
        int staticExchangeEvaluationScore = moveOrderingScore < 1525 ? moveOrderingScore - 1525 : runStaticExchangeEvaluation(inputBoard, move);
        if (
          j > 5 * depth || 
          futilityMargin < alpha || 
          staticExchangeEvaluationScore < staticExchangeEvaluationLossMargin
        ) {
          takeHashKeyMove(inputBoard);
          continue;
        }
      }        
      makeAccumulatorMove(inputBoard, move);
      makeMove(inputBoard, move);
      inputBoard.nodes++;
      int score;
      currentPrincipalVariation.clear();
      if (j == 0) {
        score = -negamax(inputBoard, depth - 1, -beta, -alpha, currentPrincipalVariation, isMoveResultingInCheck);
      } else {
        if (i == 0) startingSearch(inputBoard, depth);
        double reduction = std::log(depth) * std::log(j) / 3.0;
        if (!isMoveNoisy) reduction *= 2.0;
        reduction = std::min(int(std::round(reduction)), std::max(depth - 2, 0));
        score = reduction == 0 ? alpha + 1 : -negamax(inputBoard, depth - 1 - reduction, -alpha - 1, -alpha, currentPrincipalVariation, isMoveResultingInCheck);
        if (score > alpha) {
          currentPrincipalVariation.clear();
          score = -negamax(inputBoard, depth - 1, -alpha - 1, -alpha, currentPrincipalVariation, isMoveResultingInCheck);
          if (i == 0) endingSearch(inputBoard, depth);
          if (score > alpha && score < beta) {
            currentPrincipalVariation.clear();
            score = -negamax(inputBoard, depth - 1, -beta, -alpha, currentPrincipalVariation, isMoveResultingInCheck);
          }
        } else if (i == 0) endingSearch(inputBoard, depth);
      }
      takeHashKeyMove(inputBoard);
      takeAccumulatorMove(inputBoard);
      takeMove(inputBoard, move);
      if (inputBoard.isSearchStopped) return 0;
      // Update alpha.
      if (score > alpha) {
        principalVariation.clear();
        principalVariation.push_back(move);
        for (int k = 0; k < currentPrincipalVariation.size(); k++) principalVariation.push_back(currentPrincipalVariation[k]);
        bestMove = move;
        alpha = score;
        // Check failed high cutoff.
        if (score >= beta) {
          // Get data about one and two ply ago move.
          int onePlyAgoPiece = 12;
          int onePlyAgoTo = 64;
          int twoPlyAgoPiece = 12;
          int twoPlyAgoTo = 64;
          if (inputBoard.numberOfHistoryForNull >= 1) {
            historyEntryForNull& currentHistoryEntryForNull = inputBoard.historiesForNull[inputBoard.numberOfHistoryForNull - 1];
            onePlyAgoPiece = currentHistoryEntryForNull.piece;
            onePlyAgoTo = currentHistoryEntryForNull.to;
          }
          if (inputBoard.numberOfHistoryForNull >= 2) {
            historyEntryForNull& currentHistoryEntryForNull = inputBoard.historiesForNull[inputBoard.numberOfHistoryForNull - 2];
            twoPlyAgoPiece = currentHistoryEntryForNull.piece;
            twoPlyAgoTo = currentHistoryEntryForNull.to;
          }
          bool isOnePlyAgoValid = onePlyAgoPiece != 12 && onePlyAgoTo != 64;
          bool isTwoPlyAgoValid = twoPlyAgoPiece != 12 && twoPlyAgoTo != 64;
          // We are atleast as good as beta in this position.
          insertToTable(inputBoard, move, depth, beta, lowerBound);
          if (!isTacticalMove) {
            if (inputBoard.killersTable[inputBoard.distanceToRoot][0] != move && inputBoard.killersTable[inputBoard.distanceToRoot][1] != move) {
              inputBoard.killersTable[inputBoard.distanceToRoot][1] = inputBoard.killersTable[inputBoard.distanceToRoot][0];
              inputBoard.killersTable[inputBoard.distanceToRoot][0] = move;
            }
            int piece = inputBoard.mailbox[from];
            inputBoard.history[piece][to] += increment - increment * inputBoard.history[piece][to] / 18311;
            inputBoard.onePlyHistory[onePlyAgoPiece][onePlyAgoTo][piece][to] += increment - increment * inputBoard.onePlyHistory[onePlyAgoPiece][onePlyAgoTo][piece][to] / 18311;
            inputBoard.twoPlyHistory[twoPlyAgoPiece][twoPlyAgoTo][piece][to] += increment - increment * inputBoard.twoPlyHistory[twoPlyAgoPiece][twoPlyAgoTo][piece][to] / 18311;
            for (int k = 0; k < j; k++) {
              int move = moves.moveList[k].move;
              int from = move & 0x3f;
              int to = (move & 0xfc0) >> 6;
              int type = (move & 0xc000) >> 14;
              bool isTacticalMove = type == 1 || type == 2 || inputBoard.mailbox[to] != 12;
              int piece = inputBoard.mailbox[from];
              if (!isTacticalMove) {
                if (isOnePlyAgoValid) inputBoard.onePlyHistory[onePlyAgoPiece][onePlyAgoTo][piece][to] += -increment - increment * inputBoard.onePlyHistory[onePlyAgoPiece][onePlyAgoTo][piece][to] / 18311;
                if (isTwoPlyAgoValid) inputBoard.twoPlyHistory[twoPlyAgoPiece][twoPlyAgoTo][piece][to] += -increment - increment * inputBoard.twoPlyHistory[twoPlyAgoPiece][twoPlyAgoTo][piece][to] / 18311;
                inputBoard.history[piece][to] += -increment - increment * inputBoard.history[piece][to] / 18311;
              }
            }
          }
          return beta;
        }
      }
    }
  }
  insertToTable(inputBoard, bestMove, depth, alpha, oldAlpha == alpha ? upperBound : exact);
  return alpha;
}
void searchPosition(board& inputBoard) {
  board* workerDatas = (board*)malloc(numberOfThreads * boardSize);
  int threadsToSpawn = numberOfThreads - 1;
  for (int i = 0; i < threadsToSpawn; i++) workerDatas[i] = inputBoard;
  std::vector<int> principalVariations[numberOfThreads];
  std::vector<int> mainThreadPrincipalVariation;
  std::vector<int> principalVariation;
  int currentDepth = 1;
  bool isSideToPlayInCheck = isSquareOfSideToPlayAttacked(inputBoard, msbPosition(inputBoard.bitboards[6 * inputBoard.sideToPlay]));
  while (true) {
    std::thread workerThreads[threadsToSpawn];
    for (int i = 0; i < threadsToSpawn; i++) {
      std::vector<int>& currentPrincipalVariation = principalVariations[i];
      currentPrincipalVariation.clear();
      workerThreads[i] = std::thread(negamax, std::ref(workerDatas[i]), currentDepth, -20000, 20000, std::ref(currentPrincipalVariation), isSideToPlayInCheck);
    }
    mainThreadPrincipalVariation.clear();
    int score = negamax(inputBoard, currentDepth, -20000, 20000, mainThreadPrincipalVariation, isSideToPlayInCheck);
    // Stop all the threads once main thread has finished.
    for (int i = 0; i < threadsToSpawn; i++) {
      workerDatas[i].isSearchStopped = true;
      if (workerThreads[i].joinable()) workerThreads[i].join();
    }
    // Set the threads as unstopped once they actually have stopped so that we can search again.
    for (int i = 0; i < threadsToSpawn; i++) workerDatas[i].isSearchStopped = false;
    if (inputBoard.isSearchStopped) {
      isCurrentlySearching = true;
      break;
    }
    principalVariation = mainThreadPrincipalVariation;
    std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
    int timeElapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
    std::cout << "info depth " << currentDepth << " score ";
    std::string displayedScore = std::to_string(score);
    if (std::abs(score) >= 19936) {
      int mateIn = ceil((20000.0 - (double)std::abs(score)) / 2.0);
      mateIn *= (score < 0 ? -1 : 1);
      std::cout << "mate " << mateIn << " ";
    } else {
      std::cout << "cp " << score << " "; 
    }
    int nodes = inputBoard.nodes;
    for (int i = 0; i < numberOfThreads; i++) nodes += workerDatas[i].nodes;
    int nodesPerSecond = (int)((double)nodes / ((double)timeElapsed / 1000.0));
    std::cout << "time " << timeElapsed << " nodes " << nodes << " nps " << nodesPerSecond << " pv ";
    int principalVariationSizeMinusOne = principalVariation.size() - 1;
    for (int i = 0; i <= principalVariationSizeMinusOne; i++) {
      std::string longAlgebraicMove;
      moveToLongAlgebraic(principalVariation[i], longAlgebraicMove);
      std::cout << longAlgebraicMove;
      if (i != principalVariationSizeMinusOne) std::cout << " ";
    }
    std::cout << std::endl;
    if (currentDepth == depthLimit) {
      isCurrentlySearching = false;
      break;
    }
    currentDepth++;
  }
  std::string longAlgebraicMove;
  moveToLongAlgebraic(principalVariation[0], longAlgebraicMove);
  std::cout << "bestmove " << longAlgebraicMove << std::endl;
  isCurrentlySearching = false;
}
void prepareForSearch(board& inputBoard) {
  inputBoard.isSearchStopped = false;
  inputBoard.nodes = 0;
  inputBoard.distanceToRoot = 0;
  inputBoard.numberOfAccumulatorsHistory = 0;
  startTime = std::chrono::steady_clock::now();
  isInterruptedByGui = false;
  isCurrentlySearching = true;
  for (int i = 0; i < 64; i++) {
    for (int j = 0; j < 2; j++) {
      inputBoard.killersTable[i][j] = 0;
    }
  }
  initializeAccumulators(inputBoard);
}
