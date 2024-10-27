#include <fstream>
#include <iostream>
#include "bits.h"
#include "evaluation.h"
#include "state.h"
int featureTransformerWeightsWhite[768][ACCUMULATOR_SIZE];
int featureTransformerWeightsBlack[768][ACCUMULATOR_SIZE];
int featureTransformerBiasesWhite[numberOfFeatureTransformerBiases];
int featureTransformerBiasesBlack[numberOfFeatureTransformerBiases];
int outputLayerWeights[numberOfOutputLayerWeights];
int outputLayerBiases[numberOfOutputLayerBiases];
int* weightsPointers[4] = {
  featureTransformerBiasesWhite, featureTransformerBiasesBlack, 
  outputLayerWeights, outputLayerBiases
};
int parameterCounts[4] = {
  numberOfFeatureTransformerBiases, numberOfFeatureTransformerBiases,
  numberOfOutputLayerWeights, numberOfOutputLayerBiases
};
double multiplier[4] = {
  128.0, 128.0,
  64.0, 8192.0
};
int nnuePieceIndex[12][2] = {
  {6, 0},
  {7, 1},
  {8, 2},
  {9, 3},
  {10, 4},
  {11, 5},
  {0, 6},
  {1, 7},
  {2, 8},
  {3, 9},
  {4, 10},
  {5, 11}
};
void loadNnue() {
  std::ifstream input("./nnue.txt");
  std::string parameter;
  for (int i = 0; i < numberOfFeatureTransformerWeights; i++) {
    int accumulatorIndex = i / 768;
    int featureIndex = i % 768;
    input >> parameter;
    featureTransformerWeightsWhite[featureIndex][accumulatorIndex] = int(std::round(std::stod(parameter) * 128.0));
  }
  for (int i = 0; i < numberOfFeatureTransformerWeights; i++) {
    int accumulatorIndex = i / 768;
    int featureIndex = i % 768;
    input >> parameter;
    featureTransformerWeightsBlack[featureIndex][accumulatorIndex] = int(std::round(std::stod(parameter) * 128.0));
  }
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < parameterCounts[i]; j++) {
      input >> parameter;
      weightsPointers[i][j] = int(std::round(std::stod(parameter) * multiplier[i]));
    }
  }
}
void initializeAccumulators(board& inputBoard) {
  for (int accumulatorIndex = 0; accumulatorIndex < ACCUMULATOR_SIZE; accumulatorIndex++) {
    inputBoard.whiteAccumulator[accumulatorIndex] = featureTransformerBiasesWhite[accumulatorIndex];
    inputBoard.blackAccumulator[accumulatorIndex] = featureTransformerBiasesBlack[accumulatorIndex];
  }
  uint64_t occupiedBitboard = inputBoard.bitboards[14];
  while (occupiedBitboard != 0ull) {
    int square = lsbPosition(occupiedBitboard);
    int piece = inputBoard.mailbox[square];
    for (int perspective = 0; perspective < 2; perspective++) {
      int pieceSquare = perspective == 1 ? square : square ^ 63;
      int* accumulatorPointer = perspective == 1 ? inputBoard.whiteAccumulator : inputBoard.blackAccumulator;
      int nnueInputIndex = 64 * nnuePieceIndex[piece][perspective] + pieceSquare;
      int* updateVector = perspective == 1 ? featureTransformerWeightsWhite[nnueInputIndex] : featureTransformerWeightsBlack[nnueInputIndex];
      for (int i = 0; i < ACCUMULATOR_SIZE; i++) accumulatorPointer[i] += updateVector[i];
    }
    occupiedBitboard &= occupiedBitboard - 1ull;
  }
}
void removeAccumulatorPiece(board& inputBoard, int square) {
  int pieceToRemove = inputBoard.mailbox[square];
  int* whiteUpdateVector = featureTransformerWeightsWhite[64 * nnuePieceIndex[pieceToRemove][1] + square];
  int* blackUpdateVector = featureTransformerWeightsBlack[64 * nnuePieceIndex[pieceToRemove][0] + (square ^ 63)];
  for (int i = 0; i < ACCUMULATOR_SIZE; i++) {
    inputBoard.whiteAccumulator[i] -= whiteUpdateVector[i];
    inputBoard.blackAccumulator[i] -= blackUpdateVector[i];
  }
}
void addAccumulatorPiece(board& inputBoard, int square, int piece) {
  int* whiteUpdateVector = featureTransformerWeightsWhite[64 * nnuePieceIndex[piece][1] + square];
  int* blackUpdateVector = featureTransformerWeightsBlack[64 * nnuePieceIndex[piece][0] + (square ^ 63)];
  for (int i = 0; i < ACCUMULATOR_SIZE; i++) {
    inputBoard.whiteAccumulator[i] += whiteUpdateVector[i];
    inputBoard.blackAccumulator[i] += blackUpdateVector[i];
  }
}
void moveAccumulatorPiece(board& inputBoard, int from, int to) {
  int pieceToMove = inputBoard.mailbox[from];
  int baseWhitePerspectiveNnueInputIndex = 64 * nnuePieceIndex[pieceToMove][1];
  int baseBlackPerspectiveNnueInputIndex = 64 * nnuePieceIndex[pieceToMove][0];
  int* whiteFromUpdateVector = featureTransformerWeightsWhite[baseWhitePerspectiveNnueInputIndex + from];
  int* blackFromUpdateVector = featureTransformerWeightsBlack[baseBlackPerspectiveNnueInputIndex + (from ^ 63)];
  int* whiteToUpdateVector = featureTransformerWeightsWhite[baseWhitePerspectiveNnueInputIndex + to];
  int* blackToUpdateVector = featureTransformerWeightsBlack[baseBlackPerspectiveNnueInputIndex + (to ^ 63)];
  for (int i = 0; i < ACCUMULATOR_SIZE; i++) {
    inputBoard.whiteAccumulator[i] -= whiteFromUpdateVector[i];
    inputBoard.blackAccumulator[i] -= blackFromUpdateVector[i];
    inputBoard.whiteAccumulator[i] += whiteToUpdateVector[i];
    inputBoard.blackAccumulator[i] += blackToUpdateVector[i];
  }
}
// Call this after makeMove
void makeAccumulatorMove(board& inputBoard, int move) {
  accumulatorsHistoryEntry& newestAccumulatorsHistoryEntry = inputBoard.accumulatorsHistory[inputBoard.numberOfAccumulatorsHistory++];
  std::copy(
    inputBoard.whiteAccumulator, 
    inputBoard.whiteAccumulator + ACCUMULATOR_SIZE, 
    newestAccumulatorsHistoryEntry.whiteAccumulator
  );
  std::copy(
    inputBoard.blackAccumulator, 
    inputBoard.blackAccumulator + ACCUMULATOR_SIZE, 
    newestAccumulatorsHistoryEntry.blackAccumulator
  );
  int from = move & 0x3f;
  int to = (move & 0xfc0) >> 6;
  int promotion = (move & 0x3000) >> 12;
  int type = (move & 0xc000) >> 14;
  int capturedPiece = inputBoard.mailbox[to];
  bool isCapture = capturedPiece != 12;
  if (type == 0) {
    if (isCapture) removeAccumulatorPiece(inputBoard, to);
    moveAccumulatorPiece(inputBoard, from, to);
  } else if (type == 1) {
    if (isCapture) removeAccumulatorPiece(inputBoard, to);
    int promotionPiece = 6 * inputBoard.sideToPlay + promotion + 1;
    removeAccumulatorPiece(inputBoard, from);
    addAccumulatorPiece(inputBoard, to, promotionPiece);
  } else if (type == 2) {
    int enPassantTargetSquare = to + (inputBoard.sideToPlay == 0 ? -8 : 8);
    removeAccumulatorPiece(inputBoard, enPassantTargetSquare);
    moveAccumulatorPiece(inputBoard, from, to);
  } else {
    moveAccumulatorPiece(inputBoard, from, to);
    int kingSideCastlingToSquare = inputBoard.sideToPlay == 0 ? 6 : 62;
    int fromRookSquare;
    int toRookSquare;
    if (to == kingSideCastlingToSquare) {
      fromRookSquare = inputBoard.sideToPlay == 0 ? 7 : 63;
      toRookSquare = fromRookSquare - 2;
    } else {
      fromRookSquare = inputBoard.sideToPlay == 0 ? 0 : 56;
      toRookSquare = fromRookSquare + 3;
    }
    moveAccumulatorPiece(inputBoard, fromRookSquare, toRookSquare);
  }
}
void takeAccumulatorMove(board& inputBoard) {
  accumulatorsHistoryEntry& newestAccumulatorsHistoryEntry = inputBoard.accumulatorsHistory[--inputBoard.numberOfAccumulatorsHistory];
  std::copy(
    newestAccumulatorsHistoryEntry.whiteAccumulator, 
    newestAccumulatorsHistoryEntry.whiteAccumulator + ACCUMULATOR_SIZE, 
    inputBoard.whiteAccumulator
  );
  std::copy(
    newestAccumulatorsHistoryEntry.blackAccumulator, 
    newestAccumulatorsHistoryEntry.blackAccumulator + ACCUMULATOR_SIZE, 
    inputBoard.blackAccumulator
  );
}
int evaluate(board& inputBoard) {
  // Combine accumulators
  int accumulator[combinedAccumulatorSize];
  if (inputBoard.sideToPlay == 1) {
    for (int j = 0; j < ACCUMULATOR_SIZE; j++) {
      accumulator[j] = std::max(inputBoard.whiteAccumulator[j], 0);
      accumulator[j + ACCUMULATOR_SIZE] = std::max(inputBoard.blackAccumulator[j], 0);
    }
  } else {
    for (int j = 0; j < ACCUMULATOR_SIZE; j++) {
      accumulator[j] = std::max(inputBoard.blackAccumulator[j], 0);
      accumulator[j + ACCUMULATOR_SIZE] = std::max(inputBoard.whiteAccumulator[j], 0);
    }
  }
  // Compute output
  int output = outputLayerBiases[0];
  for (int i = 0; i < combinedAccumulatorSize; i++) output += outputLayerWeights[i] * accumulator[i];
  output /= 64;
  return output;
}
