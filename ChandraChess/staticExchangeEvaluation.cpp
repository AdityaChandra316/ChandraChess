#include <csignal>
#include <iostream>
#include "bits.h"
#include "board.h"
#include "state.h"
#include "staticExchangeEvaluation.h"
// Does it in perspective of sideToPlay.
int staticPieceValues[6] = {20000, 975, 500, 325, 325, 100};
int runStaticExchangeEvaluation(board& inputBoard, int move) {
  int score = 0;
  int from = move & 0x3f;
  int to = (move & 0xfc0) >> 6;
  int promotion = (move & 0x3000) >> 12;
  int type = (move & 0xc000) >> 14;
  // We cannot lose material when we castle because otherwise castling will be illegal.
  if (type == 3) return score;
  int ourOffset = 6 * inputBoard.sideToPlay;
  int theirOffset = 6 * (inputBoard.sideToPlay ^ 1);
  int relativePiece = inputBoard.mailbox[from] - ourOffset;
  // Get attackers to this square.
  uint64_t queenBitboard = inputBoard.bitboards[1] | inputBoard.bitboards[7];
  uint64_t rookBitboard = inputBoard.bitboards[2] | inputBoard.bitboards[8];
  uint64_t bishopBitboard = inputBoard.bitboards[3] | inputBoard.bitboards[9];
  uint64_t attackers = 0ull;
  attackers |= kingAttacks[to] & (inputBoard.bitboards[0] | inputBoard.bitboards[6]);
  attackers |= (msbRayAttacks(inputBoard, to, 0) | msbRayAttacks(inputBoard, to, 2) | lsbRayAttacks(inputBoard, to, 4) | lsbRayAttacks(inputBoard, to, 6)) & (queenBitboard | rookBitboard);
  attackers |= (msbRayAttacks(inputBoard, to, 1) | msbRayAttacks(inputBoard, to, 3) | lsbRayAttacks(inputBoard, to, 5) | lsbRayAttacks(inputBoard, to, 7)) & (queenBitboard | bishopBitboard);
  attackers |= knightAttacks[to] & (inputBoard.bitboards[4] | inputBoard.bitboards[10]);
  attackers |= pawnAttacks[1][to] & inputBoard.bitboards[5];
  attackers |= pawnAttacks[0][to] & inputBoard.bitboards[11];
  // Remove piece that just attacked.
  attackers ^= bits[from];
  uint64_t rookTypeBitboard = queenBitboard | rookBitboard;
  uint64_t bishopTypeBitboard = queenBitboard | bishopBitboard;
  int capturedPiece = inputBoard.mailbox[to];
  bool isCapture = capturedPiece != 12;
  int capturedPieceType = capturedPiece - theirOffset;
  // Add any attackers that are now unblocked from piece moving.
  if (relativePiece != 4) {
    int scanRay = fromToToRays[to][from];
    uint64_t relavantSliders = (scanRay & 1) == 0 ? rookTypeBitboard : bishopTypeBitboard;
    attackers |= relavantSliders & generalRayAttacks(inputBoard, from, scanRay);
  }
  if (type == 0) {
    if (isCapture) score += staticPieceValues[capturedPieceType];
  } else if (type == 1) {
    int promotionPiece = promotion + 1;
    if (isCapture) score += staticPieceValues[capturedPieceType];
    score += staticPieceValues[promotionPiece] - staticPieceValues[5];
    relativePiece = promotionPiece;
  } else if (type == 2) {
    score += staticPieceValues[5];
    // Add any attackers that are now unblocked from En Passant target getting removed.
    uint64_t uncoveredAttackers = inputBoard.sideToPlay == 0 ? rookTypeBitboard & lsbRayAttacks(inputBoard, to - 8, 6) : rookTypeBitboard & msbRayAttacks(inputBoard, to + 8, 2);
    attackers |= uncoveredAttackers;
  }
  inputBoard.sideToPlay ^= 1;
  score -= staticExchangeEvaluation(inputBoard, to, attackers, relativePiece);
  inputBoard.sideToPlay ^= 1;
  return score; 
}
int staticExchangeEvaluation(board& inputBoard, int square, uint64_t attackers, int lastPieceToMove) {
  int score = 0;
  uint64_t dangerousAttackers = attackers & inputBoard.bitboards[12 + inputBoard.sideToPlay];
  if (dangerousAttackers != 0ull) {
    // Get lightest attacker.
    int pieceOffset = 6 * inputBoard.sideToPlay;
    int attackerSquare;
    int attackingPiece;
    for (int piece = 5; piece >= 0; piece--) {
      uint64_t attackingPieces = dangerousAttackers & inputBoard.bitboards[pieceOffset + piece];
      if (attackingPieces != 0ull) {
        attackerSquare = msbPosition(attackingPieces);
        attackingPiece = piece;
        break;
      }
    }
    // Remove piece that just attacked.
    attackers ^= bits[attackerSquare];
    // Add any attackers that are now unblocked from piece moving.
    if (attackingPiece != 4) {
      uint64_t queens = inputBoard.bitboards[1] | inputBoard.bitboards[7];
      uint64_t rooks = inputBoard.bitboards[2] | inputBoard.bitboards[8];
      uint64_t bishops = inputBoard.bitboards[3] | inputBoard.bitboards[9];
      int scanRay = fromToToRays[square][attackerSquare];
      uint64_t relavantSliders = (scanRay & 1) == 0 ? queens | rooks : queens | bishops;
      attackers |= relavantSliders & generalRayAttacks(inputBoard, attackerSquare, scanRay);
    }
    // If attackingPiece is a pawn which is about to promote change attackingPiece to a queen and add the queens value to the score.
    int promotionScore = 0;
    if (attackingPiece == 5 && oneDimensionalToTwoDimensional[square][1] == (inputBoard.sideToPlay == 0 ? 7 : 0)) {
      attackingPiece = 1;
      promotionScore = staticPieceValues[1] - staticPieceValues[5];
    }
    inputBoard.sideToPlay ^= 1;
    score = std::max(0, staticPieceValues[lastPieceToMove] + promotionScore - staticExchangeEvaluation(inputBoard, square, attackers, attackingPiece));
    inputBoard.sideToPlay ^= 1;
  }
  return score;
}
