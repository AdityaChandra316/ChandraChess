#include <algorithm>
#include <bit>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <math.h>
#include "board.h"
#include "bits.h"
#include "evaluation.h"
#include "hashKey.h"
#include "moveGenerator.h"
#include "table.h"
#include "state.h"
#include "updateState.h"
char pieces[12] = {'k', 'q', 'r', 'b', 'n', 'p', 'K', 'Q', 'R', 'B', 'N', 'P'};
char castlingStrings[4] = {'k', 'q', 'K', 'Q'};
char promotionPieces[4] = {'q', 'r', 'b', 'n'};
uint64_t blockerMasks[2] = {0x8000000000000000ull, 1ull};
void setBoard(board& inputBoard, std::string& fen) {
  std::vector<std::string> splitFen;
  int currentTokenIndex = 0;
  for (int i = 0; i < fen.size(); i++) {
    if (currentTokenIndex == splitFen.size()) {
      std::string emptyString;
      splitFen.push_back(emptyString);
    }
    char token = fen[i];
    if (token != ' ') {
      splitFen[currentTokenIndex] += token;
    } else {
      currentTokenIndex++;
    }
  }
  std::string boardString = splitFen[0];
  std::string side = splitFen[1];
  std::string castling = splitFen[2];
  std::string enPassant = splitFen[3];
  int boardSize = boardString.size();
  int index = 0;
  for (int i = 0; i < 15; i++) inputBoard.bitboards[i] = 0ull;
  for (int i = 0; i < 64; i++) inputBoard.mailbox[i] = 12;
  for (int i = 0; i < boardSize; i++) {
    char character = boardString[i];
    if (character == '/') {
      continue;
    }
    if (isdigit(character)) {
      index += (character - '0');
      continue;
    }
    uint64_t bit = bits[index];
    switch (character) {
      case 'k':
        inputBoard.bitboards[0] |= bit;
        inputBoard.bitboards[12] |= bit;
        inputBoard.mailbox[index] = 0;
        break;
      case 'q':
        inputBoard.bitboards[1] |= bit;
        inputBoard.bitboards[12] |= bit;
        inputBoard.mailbox[index] = 1;
        break;
      case 'r':
        inputBoard.bitboards[2] |= bit;
        inputBoard.bitboards[12] |= bit;
        inputBoard.mailbox[index] = 2;
        break;
      case 'b':
        inputBoard.bitboards[3] |= bit;
        inputBoard.bitboards[12] |= bit;
        inputBoard.mailbox[index] = 3;
        break;
      case 'n':
        inputBoard.bitboards[4] |= bit;
        inputBoard.bitboards[12] |= bit;
        inputBoard.mailbox[index] = 4;
        break;
      case 'p':
        inputBoard.bitboards[5] |= bit;
        inputBoard.bitboards[12] |= bit;
        inputBoard.mailbox[index] = 5;
        break;
      case 'K':
        inputBoard.bitboards[6] |= bit;
        inputBoard.bitboards[13] |= bit;
        inputBoard.mailbox[index] = 6;
        break;
      case 'Q':
        inputBoard.bitboards[7] |= bit;
        inputBoard.bitboards[13] |= bit;
        inputBoard.mailbox[index] = 7;
        break;
      case 'R':
        inputBoard.bitboards[8] |= bit;
        inputBoard.bitboards[13] |= bit;
        inputBoard.mailbox[index] = 8;
        break;
      case 'B':
        inputBoard.bitboards[9] |= bit;
        inputBoard.bitboards[13] |= bit;
        inputBoard.mailbox[index] = 9;
        break;
      case 'N':
        inputBoard.bitboards[10] |= bit;
        inputBoard.bitboards[13] |= bit;
        inputBoard.mailbox[index] = 10;
        break;
      case 'P':
        inputBoard.bitboards[11] |= bit;
        inputBoard.bitboards[13] |= bit;
        inputBoard.mailbox[index] = 11;
        break;
    }
    inputBoard.bitboards[14] |= bit;
    index++;
  }
  inputBoard.sideToPlay = side == "b" ? 0 : 1;
  inputBoard.numberOfHistory = 0;
  inputBoard.numberOfHistoryForNull = 0;
  inputBoard.numberOfHashKeyHistory = 0;
  if (enPassant == "-") {
    inputBoard.enPassantSquare = 0;
  } else {
    char rank = enPassant[0];
    char file = enPassant[1];
    inputBoard.enPassantSquare = 8 * (8 - (file - '0')) + rank - 'a';
  }
  inputBoard.castlingPermission = 0;
  int castlingSize = castling.size();
  for (int i = 0; i < castlingSize; i++) {
    char character = castling[i];
    switch (character) {
      case 'k':
        inputBoard.castlingPermission |= castlingPermissions[0];
        break;
      case 'q':
        inputBoard.castlingPermission |= castlingPermissions[1];
        break;
      case 'K':
        inputBoard.castlingPermission |= castlingPermissions[2];
        break;
      case 'Q':
        inputBoard.castlingPermission |= castlingPermissions[3];
        break;
    }
  }
  if (splitFen.size() > 4) {
    inputBoard.halfMoveClock = stoi(splitFen[4]);
    inputBoard.halfMovesDone = 2 * (stoi(splitFen[5]) - 1) + (inputBoard.sideToPlay ^ 1);
  } else {
    inputBoard.halfMoveClock = 0;
    inputBoard.halfMovesDone = 0;
  }
  inputBoard.currentKey = getKey(inputBoard);
}
uint64_t msbRayAttacks(board& inputBoard, int square, int ray) {
  uint64_t attacks = rayAttacks[ray][square];
  uint64_t blockers = attacks & inputBoard.bitboards[14];
  int blockerSquare = msbPosition(blockers | 1ull);
  return attacks ^ rayAttacks[ray][blockerSquare];
};
uint64_t lsbRayAttacks(board& inputBoard, int square, int ray) {
  uint64_t attacks = rayAttacks[ray][square];
  uint64_t blockers = attacks & inputBoard.bitboards[14];
  int blockerSquare = lsbPosition(blockers | 0x8000000000000000ull);
  return attacks ^ rayAttacks[ray][blockerSquare];
};
// We have lots of cases where we don't know the type of ray beforehand.
uint64_t generalRayAttacks(board& inputBoard, int square, int ray) {
  bool isBitscanReverse = ray < 4;
  uint64_t attacks = rayAttacks[ray][square];
  uint64_t blockers = (attacks & inputBoard.bitboards[14]) | blockerMasks[isBitscanReverse];
  uint64_t rMask = -(uint64_t)isBitscanReverse;
  blockers &= -blockers | rMask;
  int blockerSquare = msbPosition(blockers); 
  return attacks ^ rayAttacks[ray][blockerSquare];
}
bool isSquareOfSideToPlayAttacked(board& inputBoard, int square) {
  int theirOffset = 6 * (inputBoard.sideToPlay ^ 1);
  uint64_t theirKingBitboard = inputBoard.bitboards[theirOffset];
  uint64_t theirQueenBitboard = inputBoard.bitboards[theirOffset + 1];
  uint64_t theirRookBitboard = inputBoard.bitboards[theirOffset + 2];
  uint64_t theirBishopBitboard = inputBoard.bitboards[theirOffset + 3];
  if ((kingAttacks[square] & theirKingBitboard) != 0ull) return true;
  if (((msbRayAttacks(inputBoard, square, 0) | msbRayAttacks(inputBoard, square, 2) | lsbRayAttacks(inputBoard, square, 4) | lsbRayAttacks(inputBoard, square, 6)) & (theirQueenBitboard | theirRookBitboard)) != 0ull) return true;
  if (((msbRayAttacks(inputBoard, square, 1) | msbRayAttacks(inputBoard, square, 3) | lsbRayAttacks(inputBoard, square, 5) | lsbRayAttacks(inputBoard, square, 7)) & (theirQueenBitboard | theirBishopBitboard)) != 0ull) return true;
  if ((knightAttacks[square] & inputBoard.bitboards[theirOffset + 4]) != 0ull) return true;
  if ((pawnAttacks[inputBoard.sideToPlay][square] & inputBoard.bitboards[theirOffset + 5]) != 0ull) return true;
  return false;
}
int longAlgebraicToMove(std::string& longAlgebraic, board& inputBoard) {
  int move = 0;
  int fromX = longAlgebraic[0] - 'a';
  int fromY = '8' - longAlgebraic[1];
  int toX = longAlgebraic[2] - 'a';
  int toY = '8' - longAlgebraic[3];
  int from = 8 * fromY + fromX;
  int to = 8 * toY + toX;
  move |= from | (to << 6);
  int type = 0; 
  int fromPiece = inputBoard.mailbox[from] % 6;
  if (longAlgebraic.size() == 5) {
    int promotionPiece = std::find(promotionPieces, promotionPieces + 4, longAlgebraic[4]) - promotionPieces;
    move |= promotionPiece << 12;
    type = 1;
  } else if (fromX != toX && inputBoard.mailbox[to] == 12 && fromPiece == 5) {
    type = 2;
  } else if (fromPiece == 0 && std::abs(fromX - toX) > 1) {
    type = 3;
  }
  move |= type << 14;
  return move;
}
void moveToLongAlgebraic(int move, std::string& longAlgebraicMove) {
  int from = move & 0x3f;
  int to = (move & 0xfc0) >> 6;
  int fromX = oneDimensionalToTwoDimensional[from][0];
  int fromY = oneDimensionalToTwoDimensional[from][1];
  int toX = oneDimensionalToTwoDimensional[to][0];
  int toY = oneDimensionalToTwoDimensional[to][1];
  int promotion = (move & 0x3000) >> 12;
  int type = (move & 0xc000) >> 14;
  longAlgebraicMove += 'a' + fromX;
  longAlgebraicMove += 8 - (fromY - '0');
  longAlgebraicMove += 'a' + toX;
  longAlgebraicMove += 8 - (toY - '0');
  if (type == 1) longAlgebraicMove += promotionPieces[promotion];
}
void printBoard(board& inputBoard) {
  std::cout << "Game Board: " << std::endl;
  std::cout << std::endl;
  for (int y = 0; y < 8; y++) {
    std::cout << (8 - y) << "  ";
    for (int x = 0; x < 8; x++) {
      int square = 8 * y + x;
      int piece = inputBoard.mailbox[square];
      if (piece == 12) { std::cout << ". "; } else { std::cout << pieces[piece] << " "; };
    }
    std::cout << std::endl;
  }
  std::cout << std::endl;
  std::cout << "   ";
  for (int x = 0; x < 8; x++) std::cout << char('a' + x) << " ";
  std::cout << std::endl;
  std::cout << "side:" << (inputBoard.sideToPlay == 0 ? 'b' : 'w') << std::endl;
  std::cout << "enPas:" << inputBoard.enPassantSquare << std::endl;
  std::cout << "castle:";
  for (int i = 0; i < 4; i++) {
    if ((inputBoard.castlingPermission & castlingPermissions[i]) != 0) {
      std::cout << castlingStrings[i];
    }
  }
  std::cout << std::endl;
  std::cout << "PosKey:" << inputBoard.currentKey << std::endl;
  std::cout << std::endl;
}
bool isRepetition(board& inputBoard) {
  int numberOfRepetitions = 0;
  for (int i = inputBoard.numberOfHashKeyHistory - 4; i >= inputBoard.numberOfHashKeyHistory - inputBoard.halfMoveClock; i -= 2) {
    if (inputBoard.hashKeyHistory[i] == inputBoard.currentKey) {
      numberOfRepetitions++;
    }
  }
  return (inputBoard.distanceToRoot == 1 && numberOfRepetitions == 2) || (inputBoard.distanceToRoot >= 2 && numberOfRepetitions == 1);
}
int algebraicToMove(std::string& algebraic, board& inputBoard) {
  int from;
  int to;
  int promotionPiece = 0;
  int type = 0;
  if (algebraic[0] == 'O') {
    if (algebraic.size() < 5) {
      from = inputBoard.sideToPlay == 0 ? 4 : 60;
      to = from + 2;
      type = 3;
    } else {
      from = inputBoard.sideToPlay == 0 ? 4 : 60;
      to = from - 2;
      type = 3;
    }
  } else {
    char firstToken = algebraic[0];
    char* pieceType = std::find(pieces + 6, pieces + 11, firstToken);
    bool isPawnMove = pieceType == pieces + 11;
    int captureTokenIndex = std::find(algebraic.begin(), algebraic.end(), 'x') - algebraic.begin();
    bool isCapture = captureTokenIndex != algebraic.size();
    int theirSide = inputBoard.sideToPlay ^ 1;
    int ourOffset = 6 * inputBoard.sideToPlay;
    if (isPawnMove) {
      uint64_t ourPawnBitboard = inputBoard.bitboards[ourOffset + 5];
      int firstTokenFile = firstToken - 'a';
      if (isCapture) {
        // firstTokenFile is fromX
        int toX = algebraic[2] - 'a';
        int toY = '8' - algebraic[3];
        to = 8 * toY + toX;
        from = msbPosition(pawnAttacks[theirSide][to] & files[firstTokenFile] & ourPawnBitboard);
        if ((inputBoard.bitboards[14] & bits[to]) == 0ull) type = 2;
      } else {
        int pawnMoveReverseDirection = 2 * inputBoard.sideToPlay - 1;
        int doublePushTargetRank = inputBoard.sideToPlay == 0 ? 3 : 4;
        int toY = '8' - algebraic[1];
        to = 8 * toY + firstTokenFile; // firstTokenFile is toX
        int fromIfSinglePush = to + 8 * pawnMoveReverseDirection;
        if ((ourPawnBitboard & bits[fromIfSinglePush]) != 0ull) {
          from = fromIfSinglePush;
        } else if (toY == doublePushTargetRank) {
          int fromIfDoublePush = to + 16 * pawnMoveReverseDirection;
          if ((ourPawnBitboard & bits[fromIfDoublePush]) != 0ull) from = fromIfDoublePush;
        }
      }
      int promotionTokenIndex = std::find(algebraic.begin(), algebraic.end(), '=') - algebraic.begin();
      if (promotionTokenIndex != algebraic.size()) {
        promotionPiece = std::find(pieces + 7, pieces + 11, algebraic[promotionTokenIndex + 1]) - (pieces + 7);
        type = 1;
      }
    } else {
      int relativePiece = pieceType - (pieces + 6);
      char lastToken = algebraic[algebraic.size() - 1];
      bool isCheckingMove = lastToken == '+';
      bool isCheckMatingMove = lastToken == '#';
      int filesAndRanksGiven = algebraic.size() - 3 - isCapture - isCheckingMove - isCheckMatingMove;
      int toFileIndex = filesAndRanksGiven + isCapture + 1;
      to = 8 * ('8' - algebraic[toFileIndex + 1]) + (algebraic[toFileIndex] - 'a');
      uint64_t exclusionMask = 0xffffffffffffffffull;
      if (filesAndRanksGiven == 1) exclusionMask &= (isdigit(algebraic[1]) ? ranks['8' - algebraic[1]] : files[algebraic[1] - 'a']);
      if (filesAndRanksGiven == 2) {
        from = 8 * ('8' - algebraic[2]) + (algebraic[1] - 'a');
      } else {
        movesContainer moves;
        generateMoves(inputBoard, moves, false);
        for (int i = 0; i < moves.numberOfMoves; i++) {
          moveEntry& moveStructure = moves.moveList[i];
          int move = moveStructure.move;
          int moveFrom = move & 0x3f;
          uint64_t fromBit = bits[moveFrom];
          int moveTo = (move & 0xfc0) >> 6;
          if ((fromBit & inputBoard.bitboards[ourOffset + relativePiece]) != 0ull && (fromBit & exclusionMask) != 0ull && to == moveTo) {
            from = moveFrom;
            break;
          }
        }
      }
    }
  }
  return from | (to << 6) | (promotionPiece << 12) | (type << 14);
}
void boardToFen(board& inputBoard, std::string& fen) {
  int emptySquares = 0;
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      int square = 8 * y + x;
      if ((inputBoard.bitboards[14] & bits[square]) == 0ull) {
        emptySquares++;
      } else {
        char piece = pieces[inputBoard.mailbox[square]];
        if (emptySquares != 0) {
          fen += '0' + emptySquares;
          emptySquares = 0;
        }
        fen += piece;
      }
    }
    if (emptySquares != 0) fen += '0' + emptySquares;
    emptySquares = 0;
    if (y != 7) fen += '/';
  }
  fen += ' ';
  fen += (inputBoard.sideToPlay == 0 ? 'b' : 'w');
}
bool isMoveCausingCheck(board& inputBoard, int move) {
  int from = move & 0x3f;
  int to = (move & 0xfc0) >> 6;
  int promotion = (move & 0x3000) >> 12;
  int type = (move & 0xc000) >> 14;
  int ourOffset = 6 * inputBoard.sideToPlay;
  int relativePiece = inputBoard.mailbox[from] - ourOffset;
  uint64_t fromBit = bits[from];
  uint64_t toBit = bits[to];
  int theirSide = inputBoard.sideToPlay ^ 1;
  int theirOffset = 6 * theirSide;
  uint64_t theirKingBitboard = inputBoard.bitboards[theirOffset];
  int theirKingSquare = msbPosition(theirKingBitboard);
  if (type == 2) {
    int ourPawnIndex = ourOffset + 5;
    uint64_t theirPawnSquareBit = bits[to + 8 * (2 * inputBoard.sideToPlay - 1)];
    // Removing their pawn.
    inputBoard.bitboards[14] ^= theirPawnSquareBit;
    // Making our pawn's move
    inputBoard.bitboards[14] ^= fromBit;
    inputBoard.bitboards[14] |= toBit;
    inputBoard.bitboards[ourPawnIndex] ^= fromBit;
    inputBoard.bitboards[ourPawnIndex] |= toBit;
    // Swap sides and see if En Passant move causes check.
    inputBoard.sideToPlay ^= 1;
    bool isEnPassantMoveCausingCheck = isSquareOfSideToPlayAttacked(inputBoard, theirKingSquare);
    inputBoard.sideToPlay ^= 1;
    // Adding their pawn.
    inputBoard.bitboards[14] |= theirPawnSquareBit;
    // Taking our pawn's move
    inputBoard.bitboards[14] |= fromBit;
    inputBoard.bitboards[14] ^= toBit;
    inputBoard.bitboards[ourPawnIndex] |= fromBit;
    inputBoard.bitboards[ourPawnIndex] ^= toBit;
    if (isEnPassantMoveCausingCheck) return true;
  } else if (type == 3) {
    // The only way a castling move can give check is if the rook attacks the opponent king.
    int kingSideCastlingToSquare = inputBoard.sideToPlay == 0 ? 6 : 62;
    if (inputBoard.sideToPlay == 0) {
      int rookToSquare = to == kingSideCastlingToSquare ? 5 : 3;
      if ((msbRayAttacks(inputBoard, rookToSquare, 2) & theirKingBitboard) != 0ull) return true;
    } else {
      int rookToSquare = to == kingSideCastlingToSquare ? 61 : 59;
      if ((lsbRayAttacks(inputBoard, rookToSquare, 6) & theirKingBitboard) != 0ull) return true;
    }
  } else {
    int64_t ourQueenBitboard = inputBoard.bitboards[ourOffset + 1];
    uint64_t ourRookBitboard = inputBoard.bitboards[ourOffset + 2];
    uint64_t ourBishopBitboard = inputBoard.bitboards[ourOffset + 3];
    uint64_t rookTypeBitboard = ourQueenBitboard | ourRookBitboard;
    uint64_t bishopTypeBitboard = ourQueenBitboard | ourBishopBitboard;
    int scanRay = fromToToRays[to][theirKingSquare];
    int scanRayType = scanRay & 1;
    if (type == 1) relativePiece = promotion + 1;
    // Check from regular move.
    if (relativePiece == 5) {
      if ((pawnAttacks[theirSide][theirKingSquare] & toBit) != 0ull) return true;
    } else if (relativePiece == 4) {
      if ((knightAttacks[theirKingSquare] & toBit) != 0ull) return true;
    } else {
      if (scanRay != 8 && (((relativePiece == 1 || relativePiece == 2) && scanRayType == 0) || ((relativePiece == 1 || relativePiece == 3) && scanRayType == 1))) {
        inputBoard.bitboards[14] ^= fromBit; // Removing and adding back the attacking piece ensures that the same piece on the previous move doesn't block check.
        uint64_t pieceAttacks = generalRayAttacks(inputBoard, to, scanRay);
        inputBoard.bitboards[14] |= fromBit;
        if ((pieceAttacks & theirKingBitboard) != 0ull) return true;
      }
    }
    // Other cases except standard pawn attack.
    // Piece creates a discovered check on the opponent king.
    int pinRay = fromToToRays[from][theirKingSquare];
    if (pinRay != 8 && (generalRayAttacks(inputBoard, from, pinRay) & theirKingBitboard) != 0ull) { // Check if the piece is even capable of being pinned.
      int oppositePinRay = oppositeRays[pinRay];
      int movementRay = fromToToRays[from][to];
      if (pinRay != movementRay && oppositePinRay != movementRay) { // Check if we aren't moving along pin ray.
        uint64_t relavantSliders = (pinRay & 1) == 0 ? rookTypeBitboard : bishopTypeBitboard;
        if ((relavantSliders & generalRayAttacks(inputBoard, from, oppositePinRay)) != 0ull) return true;
      }
    }
  }
  return false;
}
