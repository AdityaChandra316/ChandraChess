#include <algorithm>
#include <cstdint>
#include <iostream>
#include <math.h>
#include "evaluation.h"
#include "updateState.h"
#include "bits.h"
#include "state.h"
#include "table.h"
#include "hashKey.h"
#include "board.h"
int castlingPermissionMasks[64] = {
  0b1011, 0b1111, 0b1111, 0b1111, 0b0011, 0b1111, 0b1111, 0b0111,
  0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111,
  0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111,
  0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111,
  0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111,
  0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111,
  0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111, 0b1111,
  0b1110, 0b1111, 0b1111, 0b1111, 0b1100, 0b1111, 0b1111, 0b1101
};
void removePiece(board& inputBoard, int square) {
  uint64_t squareBit = bits[square];
  int pieceToRemove = inputBoard.mailbox[square];
  int sideOfPieceToRemove = pieceToRemove < 6 ? 0 : 1;
  inputBoard.bitboards[pieceToRemove] ^= squareBit;
  inputBoard.bitboards[12 + sideOfPieceToRemove] ^= squareBit;
  inputBoard.bitboards[14] ^= squareBit;
  inputBoard.mailbox[square] = 12;
}
void addPiece(board& inputBoard, int square, int piece) {
  uint64_t squareBit = bits[square];
  int sideOfPieceToAdd = piece < 6 ? 0 : 1;
  inputBoard.bitboards[piece] |= squareBit;
  inputBoard.bitboards[12 + sideOfPieceToAdd] |= squareBit;
  inputBoard.bitboards[14] |= squareBit;
  inputBoard.mailbox[square] = piece;
}
void movePiece(board& inputBoard, int from, int to) {
  int pieceToMove = inputBoard.mailbox[from];
  int sideOfPieceToMove = pieceToMove < 6 ? 0 : 1;
  uint64_t fromBit = bits[from];
  uint64_t toBit = bits[to];
  int ourSideBitboardIndex = 12 + sideOfPieceToMove;
  inputBoard.bitboards[pieceToMove] ^= fromBit;
  inputBoard.bitboards[ourSideBitboardIndex] ^= fromBit;
  inputBoard.bitboards[14] ^= fromBit;
  inputBoard.bitboards[pieceToMove] |= toBit;
  inputBoard.bitboards[ourSideBitboardIndex] |= toBit;
  inputBoard.bitboards[14] |= toBit;
  inputBoard.mailbox[from] = 12;
  inputBoard.mailbox[to] = pieceToMove;
}
void makeMove(board& inputBoard, int move) {
  int from = move & 0x3f;
  int piece = inputBoard.mailbox[from];
  int to = (move & 0xfc0) >> 6;
  historyEntry& newestHistoryEntry = inputBoard.histories[inputBoard.numberOfHistory++];
  newestHistoryEntry.castlingPermission = inputBoard.castlingPermission;
  newestHistoryEntry.pieceCapturedOnNextMove = 12;
  newestHistoryEntry.halfMoveClock = inputBoard.halfMoveClock;
  historyEntryForNull& newestHistoryEntryForNull = inputBoard.historiesForNull[inputBoard.numberOfHistoryForNull++];
  newestHistoryEntryForNull.enPassantSquare = inputBoard.enPassantSquare;
  newestHistoryEntryForNull.piece = piece;
  newestHistoryEntryForNull.to = to;
  bool isResettingEnPassantSquare = true;
  int promotion = (move & 0x3000) >> 12;
  int type = (move & 0xc000) >> 14;
  int ourOffset = 6 * inputBoard.sideToPlay;
  inputBoard.castlingPermission &= castlingPermissionMasks[from];
  inputBoard.castlingPermission &= castlingPermissionMasks[to];
  int capturedPiece = inputBoard.mailbox[to];
  bool isCapture = capturedPiece != 12;
  if (isCapture) newestHistoryEntry.pieceCapturedOnNextMove = capturedPiece;
  bool isPiecePawn = piece == ourOffset + 5;
  if (isCapture || isPiecePawn) { inputBoard.halfMoveClock = 0; } else { inputBoard.halfMoveClock++; };
  if (type == 0) {
    int fromSquareToDoublePushToSquare = inputBoard.sideToPlay == 0 ? 16 : -16;
    if (from + fromSquareToDoublePushToSquare == to && isPiecePawn) {
      int fromSquareToEnPassantSquare = inputBoard.sideToPlay == 0 ? 8 : -8;
      inputBoard.enPassantSquare = from + fromSquareToEnPassantSquare;
      isResettingEnPassantSquare = false;
    }
    if (isCapture) removePiece(inputBoard, to);
    movePiece(inputBoard, from, to);
  } else if (type == 1) {
    if (isCapture) removePiece(inputBoard, to);
    int promotionPiece = ourOffset + promotion + 1;
    removePiece(inputBoard, from);
    addPiece(inputBoard, to, promotionPiece);
  } else if (type == 2) {
    int enPassantTargetSquare = to + (inputBoard.sideToPlay == 0 ? -8 : 8);
    removePiece(inputBoard, enPassantTargetSquare);
    movePiece(inputBoard, from, to);
  } else {
    movePiece(inputBoard, from, to);
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
    movePiece(inputBoard, fromRookSquare, toRookSquare);
  }
  if (isResettingEnPassantSquare == true) inputBoard.enPassantSquare = 0;
  inputBoard.sideToPlay ^= 1;
  inputBoard.distanceToRoot++;
  inputBoard.halfMovesDone++;
}
void takeMove(board& inputBoard, int move) {
  inputBoard.sideToPlay ^= 1;
  historyEntry& newestHistoryEntry = inputBoard.histories[--inputBoard.numberOfHistory];
  inputBoard.castlingPermission = newestHistoryEntry.castlingPermission;
  inputBoard.halfMoveClock = newestHistoryEntry.halfMoveClock;  
  inputBoard.enPassantSquare = inputBoard.historiesForNull[--inputBoard.numberOfHistoryForNull].enPassantSquare;
  int capturedPiece = newestHistoryEntry.pieceCapturedOnNextMove;
  int from = move & 0x3f;
  int to = (move & 0xfc0) >> 6;
  int type = (move & 0xc000) >> 14;
  if (type == 0) {
    movePiece(inputBoard, to, from);
    if (capturedPiece != 12) addPiece(inputBoard, to, capturedPiece);
  } else if (type == 1) {
    removePiece(inputBoard, to);
    if (capturedPiece != 12) addPiece(inputBoard, to, capturedPiece);
    int ourPawn = 6 * inputBoard.sideToPlay + 5;
    addPiece(inputBoard, from, ourPawn);
  } else if (type == 2) {
    int enPassantTargetSquare = to + (inputBoard.sideToPlay == 0 ? -8 : 8);
    int theirPawn = 6 * (inputBoard.sideToPlay ^ 1) + 5;
    addPiece(inputBoard, enPassantTargetSquare, theirPawn);
    movePiece(inputBoard, to, from);
  } else {
    movePiece(inputBoard, to, from); 
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
    movePiece(inputBoard, toRookSquare, fromRookSquare);
  }
  inputBoard.distanceToRoot--;
  inputBoard.halfMovesDone--;
}
void makeNullMove(board& inputBoard) {
  historyEntryForNull& newestHistoryEntryForNull = inputBoard.historiesForNull[inputBoard.numberOfHistoryForNull++];
  newestHistoryEntryForNull.enPassantSquare = inputBoard.enPassantSquare;
  newestHistoryEntryForNull.piece = 12;
  newestHistoryEntryForNull.to = 64;
  inputBoard.hashKeyHistory[inputBoard.numberOfHashKeyHistory++] = inputBoard.currentKey;
  inputBoard.currentKey ^= enPassantSquareKeys[inputBoard.enPassantSquare];
  inputBoard.currentKey ^= sideToPlayKeys[inputBoard.sideToPlay];
  inputBoard.enPassantSquare = 0;
  inputBoard.sideToPlay ^= 1;
  inputBoard.currentKey ^= enPassantSquareKeys[inputBoard.enPassantSquare];
  inputBoard.currentKey ^= sideToPlayKeys[inputBoard.sideToPlay];
  inputBoard.distanceToRoot++;
  inputBoard.halfMovesDone++;
}
void takeNullMove(board& inputBoard) {
  inputBoard.enPassantSquare = inputBoard.historiesForNull[--inputBoard.numberOfHistoryForNull].enPassantSquare;
  inputBoard.currentKey = inputBoard.hashKeyHistory[--inputBoard.numberOfHashKeyHistory];
  inputBoard.sideToPlay ^= 1;
  inputBoard.distanceToRoot--;
  inputBoard.halfMovesDone--;
}
