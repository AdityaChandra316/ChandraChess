#include <algorithm>
#include <bit>
#include <cstdint>
#include <iostream>
#include <math.h>
#include <vector>
#include "moveGenerator.h"
#include "bits.h"
#include "state.h"
#include "table.h"
#include "board.h"
#include "staticExchangeEvaluation.h"
int rayStarts[3] = {0, 0, 1};
int rayIncrements[3] = {1, 2, 2};
bool isRayTypeValid[3][2] = {
  {true, true},
  {true, false},
  {false, true}
};
void saveMove(int from, int to, int promotionPiece, int type, movesContainer& moves) {
  moves.moveList[moves.numberOfMoves++].move = from | (to << 6) | (promotionPiece << 12) | (type << 14);
}
bool compareScoredMoves(moveEntry& firstScoredMove, moveEntry& secondScoredMove) {
  return firstScoredMove.score > secondScoredMove.score;
}
// orderMoves() scores the moves based on various move ordering techniques.
void orderMoves(board& inputBoard, movesContainer& moves, int hashMove) {
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
  // Score all the moves.
  for (int i = 0; i < moves.numberOfMoves; i++) {
    moveEntry& currentMove = moves.moveList[i];
    int move = currentMove.move;
    int from = move & 0x3f;
    int to = (move & 0xfc0) >> 6;
    int promotion = (move & 0x3000) >> 12;
    int type = (move & 0xc000) >> 14;
    if (hashMove == move) {
      currentMove.score = 40000;
    } else if (type == 1 || type == 2 || inputBoard.mailbox[to] != 12) {
      int score = runStaticExchangeEvaluation(inputBoard, move);
      if (score >= 0) {
        // Winning capture.
        // If capture score is 1850 move ordering score will be 39999 (pawn takes a queen and promotes).
        // If capture score is 0 move ordering score will be 38149.
        currentMove.score = 38149 + score;
      } else {
        // Losing captures.
        // If capture score is -1525 move ordering score is 0 (queen takes a minor piece and gets captured by a promoting pawn).
        // If capture score is -1 move ordering score is 1524.
        currentMove.score = 1525 + score;
      }
    } else {
      if (inputBoard.killersTable[inputBoard.distanceToRoot][0] == move || inputBoard.killersTable[inputBoard.distanceToRoot][1] == move) {
        currentMove.score = 38148;
      } else {
        int piece = inputBoard.mailbox[from];
        int historyScore = (
          inputBoard.history[piece][to] + 
          2 * (isOnePlyAgoValid ? inputBoard.onePlyHistory[onePlyAgoPiece][onePlyAgoTo][piece][to] : 0) +
          (isTwoPlyAgoValid ? inputBoard.twoPlyHistory[twoPlyAgoPiece][twoPlyAgoTo][piece][to] : 0)
        ) / 4;
        currentMove.score = 19836 + historyScore;
      }
    }
  }
  std::sort(moves.moveList, moves.moveList + moves.numberOfMoves, compareScoredMoves);
}
void generateMoves(board& inputBoard, movesContainer& moves, bool isQuiescentGenerator) {
  int ourOffset = 6 * inputBoard.sideToPlay;
  int kingSquare = msbPosition(inputBoard.bitboards[ourOffset]);
  uint64_t kingSquareBit = bits[kingSquare];
  uint64_t ourPawnBitboard = inputBoard.bitboards[ourOffset + 5];
  uint64_t ourBitboard = inputBoard.bitboards[12 + inputBoard.sideToPlay];
  int theirSide = inputBoard.sideToPlay ^ 1;
  int theirOffset = 6 * theirSide;
  uint64_t theirKingBitboard = inputBoard.bitboards[theirOffset];
  uint64_t theirQueenBitboard = inputBoard.bitboards[theirOffset + 1];
  uint64_t theirRookBitboard = inputBoard.bitboards[theirOffset + 2];
  int theirPawnIndex = theirOffset + 5;
  uint64_t theirBishopBitboard = inputBoard.bitboards[theirOffset + 3];
  uint64_t theirKnightBitboard = inputBoard.bitboards[theirOffset + 4];
  uint64_t theirPawnBitboard = inputBoard.bitboards[theirPawnIndex];
  uint64_t theirBitboard = inputBoard.bitboards[12 + theirSide];
  int theirKingSquare = msbPosition(theirKingBitboard);
  uint64_t kingRays[8] = {
    msbRayAttacks(inputBoard, kingSquare, 0), msbRayAttacks(inputBoard, kingSquare, 1), msbRayAttacks(inputBoard, kingSquare, 2), msbRayAttacks(inputBoard, kingSquare, 3), 
    lsbRayAttacks(inputBoard, kingSquare, 4), lsbRayAttacks(inputBoard, kingSquare, 5), lsbRayAttacks(inputBoard, kingSquare, 6), lsbRayAttacks(inputBoard, kingSquare, 7)
  };
  // Get bitboard of checking pieces.
  uint64_t checkers = 0ull;
  checkers |= kingAttacks[kingSquare] & theirKingBitboard;
  checkers |= (kingRays[0] | kingRays[2] | kingRays[4] | kingRays[6]) & (theirQueenBitboard | theirRookBitboard);
  checkers |= (kingRays[1] | kingRays[3] | kingRays[5] | kingRays[7]) & (theirQueenBitboard | theirBishopBitboard);
  checkers |= knightAttacks[kingSquare] & theirKnightBitboard;
  checkers |= pawnAttacks[inputBoard.sideToPlay][kingSquare] & theirPawnBitboard;
  int numberOfCheckers = std::popcount(checkers);
  uint64_t slidingCheckers = checkers & (theirQueenBitboard | theirRookBitboard | theirBishopBitboard);
  // Get bitboard of squares attacked by opponent without our king.
  uint64_t squaresAttackedByOpponent = 0ull;
  inputBoard.bitboards[14] ^= kingSquareBit;
  squaresAttackedByOpponent |= kingAttacks[theirKingSquare];
  for (int piece = 0; piece < 3; piece++) {
    uint64_t pieces = inputBoard.bitboards[theirOffset + piece + 1];
    while (pieces != 0ull) {
      int pieceSquare = lsbPosition(pieces);
      for (int ray = rayStarts[piece]; ray < 8; ray += rayIncrements[piece]) squaresAttackedByOpponent |= generalRayAttacks(inputBoard, pieceSquare, ray);
      pieces &= pieces - 1ull;
    }
  }
  while (theirKnightBitboard != 0ull) {
    int attackerSquare = lsbPosition(theirKnightBitboard);
    squaresAttackedByOpponent |= knightAttacks[attackerSquare];
    theirKnightBitboard &= theirKnightBitboard - 1ull;
  }
  if (inputBoard.sideToPlay == 1) {
    squaresAttackedByOpponent |= ((theirPawnBitboard & ~files[0]) >> 7ull);
    squaresAttackedByOpponent |= ((theirPawnBitboard & ~files[7]) >> 9ull);
  } else {
    squaresAttackedByOpponent |= ((theirPawnBitboard & ~files[7]) << 7ull);
    squaresAttackedByOpponent |= ((theirPawnBitboard & ~files[0]) << 9ull);
  }
  inputBoard.bitboards[14] ^= kingSquareBit;
  // Add king moves.
  uint64_t notOurBitboard = ~ourBitboard;
  uint64_t kingMoves = kingAttacks[kingSquare] & notOurBitboard & ~squaresAttackedByOpponent;
  if (isQuiescentGenerator) kingMoves &= theirBitboard;
  while (kingMoves != 0ull) {
    int to = lsbPosition(kingMoves);
    saveMove(kingSquare, to, 0, 0, moves);
    kingMoves &= kingMoves - 1ull;
  } 
  if (numberOfCheckers == 2) return;
  // Generate moveMask.
  uint64_t pushMask = 0xffffffffffffffffull;
  uint64_t captureMask = 0xffffffffffffffffull;
  uint64_t moveMask = notOurBitboard;
  if (numberOfCheckers == 1) {
    if (slidingCheckers != 0ull) {
      int slidingCheckerSquare = msbPosition(slidingCheckers);
      int rayFromKing = fromToToRays[kingSquare][slidingCheckerSquare];
      int oppositeRay = oppositeRays[rayFromKing];
      pushMask = kingRays[rayFromKing] & generalRayAttacks(inputBoard, slidingCheckerSquare, oppositeRay);
    } else {
      pushMask = 0ull;
    }
    captureMask = checkers;
    moveMask &= (pushMask | captureMask);
  }
  // Get bitboard of pinned pieces on each ray.
  uint64_t pinnedPieces[4] = {0ull, 0ull, 0ull, 0ull};
  for (int piece = 0; piece < 3; piece++) {
    uint64_t pieces = inputBoard.bitboards[theirOffset + piece + 1];
    while (pieces != 0ull) {
      int from = lsbPosition(pieces);
      int rayFromKing = fromToToRays[kingSquare][from];
      int rayType = rayFromKing & 3;
      if (rayFromKing != 8 && isRayTypeValid[piece][rayType & 1]) {
        int oppositeRay = oppositeRays[rayFromKing];
        pinnedPieces[rayType] |= kingRays[rayFromKing] & generalRayAttacks(inputBoard, from, oppositeRay);
      }
      pieces &= pieces - 1ull;
    }
  }
  uint64_t pinnedOnHorizontal = pinnedPieces[0];
  uint64_t pinnedOnNegativeDiagonal = pinnedPieces[1];
  uint64_t pinnedOnVertical = pinnedPieces[2];
  uint64_t pinnedOnPositiveDiagonal = pinnedPieces[3];
  uint64_t notPinned = ~(pinnedOnHorizontal | pinnedOnNegativeDiagonal | pinnedOnVertical | pinnedOnPositiveDiagonal);
  // Add pawn moves.
  uint64_t emptySquares = ~inputBoard.bitboards[14];
  uint64_t pawnPushMask = ~theirBitboard & moveMask;
  uint64_t pawnCaptureMask = theirBitboard & moveMask;
  uint64_t verticallyPinnedPawns = pinnedOnVertical & ourPawnBitboard;
  uint64_t positiveDiagonallyPinnedPawns = pinnedOnPositiveDiagonal & ourPawnBitboard;
  uint64_t negativeDiagonallyPinnedPawns = pinnedOnNegativeDiagonal & ourPawnBitboard;
  uint64_t unPinnedPawns = notPinned & ourPawnBitboard; 
  uint64_t notLeftFile = ~files[0];
  uint64_t notRightFile = ~files[7];
  uint64_t singlePushes;
  uint64_t doublePushes;
  uint64_t positiveDiagonalCaptures;
  uint64_t negativeDiagonalCaptures;
  uint64_t verticallyPushablePawns = verticallyPinnedPawns | unPinnedPawns;
  int promotionY;
  int sign;
  if (inputBoard.sideToPlay == 0) {
    singlePushes = (verticallyPushablePawns >> 8ull) & pawnPushMask;
    doublePushes = ((((verticallyPushablePawns & ranks[1]) >> 8ull) & emptySquares) >> 8ull) & pawnPushMask;
    positiveDiagonalCaptures = (((positiveDiagonallyPinnedPawns | unPinnedPawns) & notLeftFile) >> 7ull) & pawnCaptureMask;
    negativeDiagonalCaptures = (((negativeDiagonallyPinnedPawns | unPinnedPawns) & notRightFile) >> 9ull) & pawnCaptureMask;
    promotionY = 7;
    sign = -1;
  } else {
    singlePushes = (verticallyPushablePawns << 8ull) & pawnPushMask;
    doublePushes = ((((verticallyPushablePawns & ranks[6]) << 8ull) & emptySquares) << 8ull) & pawnPushMask;
    positiveDiagonalCaptures = (((positiveDiagonallyPinnedPawns | unPinnedPawns) & notRightFile) << 7ull) & pawnCaptureMask;
    negativeDiagonalCaptures = (((negativeDiagonallyPinnedPawns | unPinnedPawns) & notLeftFile) << 9ull) & pawnCaptureMask;
    promotionY = 0;
    sign = 1;
  }
  uint64_t promotionRank = ranks[promotionY];
  uint64_t notPromotionRank = ~promotionRank;
  uint64_t singlePushesNonPromotion = singlePushes & notPromotionRank;
  uint64_t singlePushesPromotion = singlePushes & promotionRank;
  if (!isQuiescentGenerator) {
    while (doublePushes != 0ull) {
      int to = lsbPosition(doublePushes);
      int from = to + sign * 16;
      saveMove(from, to, 0, 0, moves);
      doublePushes &= doublePushes - 1ull;
    }
    while (singlePushesNonPromotion != 0ull) {
      int to = lsbPosition(singlePushesNonPromotion);
      int from = to + sign * 8;
      saveMove(from, to, 0, 0, moves);
      singlePushesNonPromotion &= singlePushesNonPromotion - 1ull;
    }
  }
  while (singlePushesPromotion != 0ull) {
    int to = lsbPosition(singlePushesPromotion);
    int from = to + sign * 8;
    saveMove(from, to, 0, 1, moves);
    saveMove(from, to, 1, 1, moves);
    saveMove(from, to, 2, 1, moves);
    saveMove(from, to, 3, 1, moves);
    singlePushesPromotion &= singlePushesPromotion - 1ull;
  }
  while (positiveDiagonalCaptures != 0ull) {
    int to = lsbPosition(positiveDiagonalCaptures);
    int toY = oneDimensionalToTwoDimensional[to][1];
    int from = to + sign * 7;
    if (toY == promotionY) {
      saveMove(from, to, 0, 1, moves);
      saveMove(from, to, 1, 1, moves);
      saveMove(from, to, 2, 1, moves);
      saveMove(from, to, 3, 1, moves);
    } else {
      saveMove(from, to, 0, 0, moves);
    }
    positiveDiagonalCaptures &= positiveDiagonalCaptures - 1ull;
  }
  while (negativeDiagonalCaptures != 0ull) {
    int to = lsbPosition(negativeDiagonalCaptures);
    int toY = oneDimensionalToTwoDimensional[to][1];
    int from = to + sign * 9;
    if (toY == promotionY) {
      saveMove(from, to, 0, 1, moves);
      saveMove(from, to, 1, 1, moves);
      saveMove(from, to, 2, 1, moves);
      saveMove(from, to, 3, 1, moves);
    } else {
      saveMove(from, to, 0, 0, moves);
    }
    negativeDiagonalCaptures &= negativeDiagonalCaptures - 1ull;
  }
  // Set moveMask based on isQuiescentGenerator
  if (isQuiescentGenerator) moveMask &= theirBitboard;
  // Add slider moves.
  for (int piece = 0; piece < 3; piece++) {
    uint64_t pieces = inputBoard.bitboards[ourOffset + piece + 1];
    for (int pinRay = rayStarts[piece]; pinRay < 4; pinRay += rayIncrements[piece]) {
      uint64_t pinnedPiecesOnRay = pieces & pinnedPieces[pinRay];
      while (pinnedPiecesOnRay != 0ull) {
        int from = lsbPosition(pinnedPiecesOnRay);
        uint64_t ourSliderMoves = (msbRayAttacks(inputBoard, from, pinRay) | lsbRayAttacks(inputBoard, from, pinRay + 4)) & moveMask;
        while (ourSliderMoves != 0ull) {
          int to = lsbPosition(ourSliderMoves);
          saveMove(from, to, 0, 0, moves);
          ourSliderMoves &= ourSliderMoves - 1ull;
        }
        pinnedPiecesOnRay &= pinnedPiecesOnRay - 1ull;
      }
    }
    uint64_t notPinnedPieces = pieces & notPinned;
    while (notPinnedPieces != 0ull) {
      int from = lsbPosition(notPinnedPieces);
      uint64_t ourSliderMoves = 0ull;
      for (int ray = rayStarts[piece]; ray < 8; ray += rayIncrements[piece]) ourSliderMoves |= generalRayAttacks(inputBoard, from, ray);
      ourSliderMoves &= moveMask;
      while (ourSliderMoves != 0ull) {
        int to = lsbPosition(ourSliderMoves);
        saveMove(from, to, 0, 0, moves);
        ourSliderMoves &= ourSliderMoves - 1ull;
      }
      notPinnedPieces &= notPinnedPieces - 1ull;
    }
  }
  // Add knight moves.
  uint64_t notPinnedKnights = notPinned & inputBoard.bitboards[ourOffset + 4];
  while (notPinnedKnights != 0ull) {
    int from = lsbPosition(notPinnedKnights);
    uint64_t knightMoves = knightAttacks[from] & moveMask;
    if (isQuiescentGenerator) knightMoves &= inputBoard.bitboards[14];
    while (knightMoves != 0ull) {
      int to = lsbPosition(knightMoves);
      saveMove(from, to, 0, 0, moves);
      knightMoves &= knightMoves - 1ull;
    }
    notPinnedKnights &= notPinnedKnights - 1ull;
  }
  // Add en passant moves.
  if (inputBoard.enPassantSquare != 0) {
    int enPassantX = oneDimensionalToTwoDimensional[inputBoard.enPassantSquare][0];
    int theirPawnSquare = inputBoard.enPassantSquare + 8 * (2 * inputBoard.sideToPlay - 1);
    if (enPassantX > 0) {
      int from = theirPawnSquare - 1;
      if ((bits[from] & ourPawnBitboard) != 0ull) {
        inputBoard.bitboards[theirPawnIndex] ^= bits[theirPawnSquare];
        inputBoard.bitboards[14] ^= bits[theirPawnSquare];
        inputBoard.bitboards[14] ^= bits[from];
        inputBoard.bitboards[14] |= bits[inputBoard.enPassantSquare];
        bool isInCheck = isSquareOfSideToPlayAttacked(inputBoard, kingSquare);
        inputBoard.bitboards[theirPawnIndex] |= bits[theirPawnSquare];
        inputBoard.bitboards[14] |= bits[theirPawnSquare];
        inputBoard.bitboards[14] |= bits[from];
        inputBoard.bitboards[14] ^= bits[inputBoard.enPassantSquare];
        if (!isInCheck) saveMove(from, inputBoard.enPassantSquare, 0, 2, moves);
      }
    }
    if (enPassantX < 7) {
      int from = theirPawnSquare + 1;
      if ((bits[from] & ourPawnBitboard) != 0ull) {
        inputBoard.bitboards[theirPawnIndex] ^= bits[theirPawnSquare];
        inputBoard.bitboards[14] ^= bits[theirPawnSquare];
        inputBoard.bitboards[14] ^= bits[from];
        inputBoard.bitboards[14] |= bits[inputBoard.enPassantSquare];
        bool isInCheck = isSquareOfSideToPlayAttacked(inputBoard, kingSquare);
        inputBoard.bitboards[theirPawnIndex] |= bits[theirPawnSquare];
        inputBoard.bitboards[14] |= bits[theirPawnSquare];
        inputBoard.bitboards[14] |= bits[from];
        inputBoard.bitboards[14] ^= bits[inputBoard.enPassantSquare];
        if (!isInCheck) saveMove(from, inputBoard.enPassantSquare, 0, 2, moves);
      }
    }
  }
  // Add castling moves.
  if (numberOfCheckers == 0 && !isQuiescentGenerator) {
    int castlingPermissionsOffset = 2 * inputBoard.sideToPlay;
    if ((inputBoard.castlingPermission & castlingPermissions[castlingPermissionsOffset]) != 0) {
      if ((inputBoard.bitboards[14] & bits[kingSquare + 1]) == 0ull && (inputBoard.bitboards[14] & bits[kingSquare + 2]) == 0ull) {
        if (!isSquareOfSideToPlayAttacked(inputBoard, kingSquare + 1) && !isSquareOfSideToPlayAttacked(inputBoard, kingSquare + 2)) saveMove(kingSquare, kingSquare + 2, 0, 3, moves);
      }
    }
    if ((inputBoard.castlingPermission & castlingPermissions[castlingPermissionsOffset + 1]) != 0) {
      if ((inputBoard.bitboards[14] & bits[kingSquare - 1]) == 0ull && (inputBoard.bitboards[14] & bits[kingSquare - 2]) == 0ull && (inputBoard.bitboards[14] & bits[kingSquare - 3]) == 0ull) {
        if (!isSquareOfSideToPlayAttacked(inputBoard, kingSquare - 1) && !isSquareOfSideToPlayAttacked(inputBoard, kingSquare - 2)) saveMove(kingSquare, kingSquare - 2, 0, 3, moves);
      }
    }
  }
}
