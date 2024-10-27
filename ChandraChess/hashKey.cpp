#include <cstdint>
#include <iostream>
#include <math.h>
#include <random>
#include "bits.h"
#include "hashKey.h"
#include "state.h"
#include "updateState.h"
uint64_t sideToPlayKeys[2];
uint64_t positionKeys[13][64];
uint64_t castlingPermissionsKeys[16];
uint64_t enPassantSquareKeys[64];
void initializeKeys() {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> d(0ull, 0xffffffffffffffffull);
  sideToPlayKeys[0] = d(gen);
  sideToPlayKeys[1] = d(gen);
  for (int i = 0; i < 13; i++) {
    for (int j = 0; j < 64; j++) {
      positionKeys[i][j] = d(gen);
    }
  }
  for (int i = 0; i < 16; i++) {
    castlingPermissionsKeys[i] = d(gen);
  }
  for (int i = 0; i < 64; i++) {
    enPassantSquareKeys[i] = d(gen);
  }
}
uint64_t getKey(board& inputBoard) {
  uint64_t key = 0ull;
  for (int i = 0; i < 64; i++) key ^= positionKeys[inputBoard.mailbox[i]][i];
  key ^= sideToPlayKeys[inputBoard.sideToPlay];
  key ^= castlingPermissionsKeys[inputBoard.castlingPermission];
  key ^= enPassantSquareKeys[inputBoard.enPassantSquare];
  return key;
}
void removeHashKeyPiece(board& inputBoard, int square) {
  int pieceToRemove = inputBoard.mailbox[square];
  inputBoard.currentKey ^= positionKeys[pieceToRemove][square];
  inputBoard.currentKey ^= positionKeys[12][square];
}
void addHashKeyPiece(board& inputBoard, int square, int piece) {
  inputBoard.currentKey ^= positionKeys[12][square];
  inputBoard.currentKey ^= positionKeys[piece][square];
}
void moveHashKeyPiece(board& inputBoard, int from, int to) {
  int pieceToMove = inputBoard.mailbox[from];
  inputBoard.currentKey ^= positionKeys[pieceToMove][from];
  inputBoard.currentKey ^= positionKeys[12][from];
  inputBoard.currentKey ^= positionKeys[12][to];
  inputBoard.currentKey ^= positionKeys[pieceToMove][to];
}
void makeHashKeyMove(board& inputBoard, int move) {
  inputBoard.hashKeyHistory[inputBoard.numberOfHashKeyHistory++] = inputBoard.currentKey;
  bool isResettingEnPassantSquare = true;
  int from = move & 0x3f;
  int to = (move & 0xfc0) >> 6;
  int promotion = (move & 0x3000) >> 12;
  int type = (move & 0xc000) >> 14;
  int currentSideToPlay = inputBoard.sideToPlay;
  int ourOffset = 6 * currentSideToPlay;
  int currentCastlingPermission = inputBoard.castlingPermission;
  inputBoard.currentKey ^= castlingPermissionsKeys[currentCastlingPermission];
  currentCastlingPermission &= castlingPermissionMasks[from];
  currentCastlingPermission &= castlingPermissionMasks[to];
  inputBoard.currentKey ^= castlingPermissionsKeys[currentCastlingPermission];
  int capturedPiece = inputBoard.mailbox[to];
  bool isCapture = capturedPiece != 12;
  bool isPiecePawn = inputBoard.mailbox[from] == ourOffset + 5;
  int currentEnPassantSquare = inputBoard.enPassantSquare;
  inputBoard.currentKey ^= enPassantSquareKeys[currentEnPassantSquare];
  if (type == 0) {
    int fromSquareToDoublePushToSquare = currentSideToPlay == 0 ? 16 : -16;
    if (from + fromSquareToDoublePushToSquare == to && isPiecePawn) {
      int fromSquareToEnPassantSquare = currentSideToPlay == 0 ? 8 : -8;
      currentEnPassantSquare = from + fromSquareToEnPassantSquare;
      isResettingEnPassantSquare = false;
    }
    if (isCapture) removeHashKeyPiece(inputBoard, to);
    moveHashKeyPiece(inputBoard, from, to);
  } else if (type == 1) {
    if (isCapture) removeHashKeyPiece(inputBoard, to);
    int promotionPiece = ourOffset + promotion + 1;
    removeHashKeyPiece(inputBoard, from);
    addHashKeyPiece(inputBoard, to, promotionPiece);
  } else if (type == 2) {
    int enPassantTargetSquare = to + (currentSideToPlay == 0 ? -8 : 8);
    removeHashKeyPiece(inputBoard, enPassantTargetSquare);
    moveHashKeyPiece(inputBoard, from, to);
  } else {
    moveHashKeyPiece(inputBoard, from, to);
    int kingSideCastlingToSquare = currentSideToPlay == 0 ? 6 : 62;
    int fromRookSquare;
    int toRookSquare;
    if (to == kingSideCastlingToSquare) {
      fromRookSquare = currentSideToPlay == 0 ? 7 : 63;
      toRookSquare = fromRookSquare - 2;
    } else {
      fromRookSquare = currentSideToPlay == 0 ? 0 : 56;
      toRookSquare = fromRookSquare + 3;
    }
    moveHashKeyPiece(inputBoard, fromRookSquare, toRookSquare);
  }
  if (isResettingEnPassantSquare == true) currentEnPassantSquare = 0;
  inputBoard.currentKey ^= enPassantSquareKeys[currentEnPassantSquare];
  inputBoard.currentKey ^= sideToPlayKeys[currentSideToPlay];
  currentSideToPlay ^= 1;
  inputBoard.currentKey ^= sideToPlayKeys[currentSideToPlay];
}
void takeHashKeyMove(board& inputBoard) { inputBoard.currentKey = inputBoard.hashKeyHistory[--inputBoard.numberOfHashKeyHistory]; }
