#include <iostream>
#include <bitset>
#include <cstdint>
#include <vector>
#include <algorithm>

using namespace std;

// Board constants
const int BOARD_SIZE = 8;
const uint64_t FILE_A = 0x0101010101010101ULL;
const uint64_t FILE_B = 0x0202020202020202ULL; // New definition for FILE_B
const uint64_t FILE_G = 0x4040404040404040ULL; // New definition for FILE_G
const uint64_t FILE_H = 0x8080808080808080ULL;
const uint64_t RANK_1 = 0x00000000000000FFULL;
const uint64_t RANK_2 = 0x000000000000FF00ULL;
const uint64_t RANK_4 = 0x00000000FF000000ULL;
const uint64_t RANK_5 = 0x000000FF00000000ULL;
const uint64_t RANK_7 = 0x00FF000000000000ULL;
const uint64_t RANK_8 = 0xFF00000000000000ULL;


// Piece bitboards
uint64_t whitePawns, whiteKnights, whiteBishops, whiteRooks, whiteQueens, whiteKing;
uint64_t blackPawns, blackKnights, blackBishops, blackRooks, blackQueens, blackKing;
uint64_t whitePieces, blackPieces, allPieces;

// Castling rights
bool whiteKingsideCastle = true, whiteQueensideCastle = true;
bool blackKingsideCastle = true, blackQueensideCastle = true;

// En passant target square
uint64_t enPassantTarget = 0;

// Initialize board position
void initializePosition() {
    whitePawns   = 0x000000000000FF00ULL;
    whiteKnights = 0x0000000000000042ULL;
    whiteBishops = 0x0000000000000024ULL;
    whiteRooks   = 0x0000000000000081ULL;
    whiteQueens  = 0x0000000000000008ULL;
    whiteKing    = 0x0000000000000010ULL;

    blackPawns   = 0x00FF000000000000ULL;
    blackKnights = 0x4200000000000000ULL;
    blackBishops = 0x2400000000000000ULL;
    blackRooks   = 0x8100000000000000ULL;
    blackQueens  = 0x0800000000000000ULL;
    blackKing    = 0x1000000000000000ULL;

    whitePieces = whitePawns | whiteKnights | whiteBishops | whiteRooks | whiteQueens | whiteKing;
    blackPieces = blackPawns | blackKnights | blackBishops | blackRooks | blackQueens | blackKing;
    allPieces = whitePieces | blackPieces;

    whiteKingsideCastle = whiteQueensideCastle = true;
    blackKingsideCastle = blackQueensideCastle = true;
    enPassantTarget = 0;
}

// Helper to print bitboards for testing
void printBitboard(uint64_t bitboard) {
    cout << "  a b c d e f g h\n +----------------+\n";
    for (int rank = 7; rank >= 0; --rank) {
        cout << rank + 1 << "| ";
        for (int file = 0; file < 8; ++file) {
            int square = rank * 8 + file;
            cout << (((bitboard >> square) & 1) ? "1 " : ". ");
        }
        cout << "|\n";
    }
    cout << " +----------------+\n";
}

// Sliding piece moves (rooks and bishops), with blockers
uint64_t slideMove(uint64_t piece, int direction, uint64_t blockers) {
    uint64_t moves = 0;
    uint64_t temp = piece;
    while (temp) {
        temp = (direction > 0) ? (temp << direction) : (temp >> -direction);
        if (temp & blockers) break;
        moves |= temp;
    }
    return moves;
}

// Generate rook moves
uint64_t generateRookMoves(uint64_t rooks, uint64_t occupied) {
    uint64_t moves = 0;
    moves |= slideMove(rooks, 8, occupied);  // Up
    moves |= slideMove(rooks, -8, occupied); // Down
    moves |= slideMove(rooks, 1, occupied & ~FILE_A); // Right
    moves |= slideMove(rooks, -1, occupied & ~FILE_H); // Left
    return moves;
}

// Generate bishop moves
uint64_t generateBishopMoves(uint64_t bishops, uint64_t occupied) {
    uint64_t moves = 0;
    moves |= slideMove(bishops, 9, occupied & ~FILE_A); // Up-right
    moves |= slideMove(bishops, 7, occupied & ~FILE_H); // Up-left
    moves |= slideMove(bishops, -9, occupied & ~FILE_H); // Down-left
    moves |= slideMove(bishops, -7, occupied & ~FILE_A); // Down-right
    return moves;
}

// Generate queen moves by combining rook and bishop moves
uint64_t generateQueenMoves(uint64_t queens, uint64_t occupied) {
    return generateRookMoves(queens, occupied) | generateBishopMoves(queens, occupied);
}

// Generate king moves, including castling
uint64_t generateKingMoves(uint64_t king, bool isWhite) {
    uint64_t moves = 0;
    moves |= (king << 8) | (king >> 8); // Up and down
    moves |= ((king & ~FILE_H) << 1) | ((king & ~FILE_A) >> 1); // Left and right
    moves |= ((king & ~FILE_H) << 9) | ((king & ~FILE_A) << 7); // Diagonals
    moves |= ((king & ~FILE_H) >> 7) | ((king & ~FILE_A) >> 9);

    // Castling
    if (isWhite) {
        if (whiteKingsideCastle && !(allPieces & 0x0000000000000060ULL)) moves |= 0x0000000000000040ULL;
        if (whiteQueensideCastle && !(allPieces & 0x000000000000000EULL)) moves |= 0x0000000000000004ULL;
    } else {
        if (blackKingsideCastle && !(allPieces & 0x6000000000000000ULL)) moves |= 0x4000000000000000ULL;
        if (blackQueensideCastle && !(allPieces & 0x0E00000000000000ULL)) moves |= 0x0400000000000000ULL;
    }
    return moves;
}

// Generate pawn moves, including en passant and promotion
uint64_t generatePawnMoves(uint64_t pawns, uint64_t emptySquares, bool isWhite) {
    uint64_t moves = 0;
    uint64_t promotionMoves = 0;
    if (isWhite) {
        moves |= (pawns << 8) & emptySquares; // Single move
        moves |= ((pawns & RANK_2) << 16) & emptySquares & (emptySquares << 8); // Double move
        moves |= (pawns << 9) & ~FILE_H & (blackPieces | enPassantTarget); // Capture right
        moves |= (pawns << 7) & ~FILE_A & (blackPieces | enPassantTarget); // Capture left
        promotionMoves = moves & RANK_8; // Promotion on 8th rank
    } else {
        moves |= (pawns >> 8) & emptySquares; // Single move
        moves |= ((pawns & RANK_7) >> 16) & emptySquares & (emptySquares >> 8); // Double move
        moves |= (pawns >> 9) & ~FILE_A & (whitePieces | enPassantTarget); // Capture left
        moves |= (pawns >> 7) & ~FILE_H & (whitePieces | enPassantTarget); // Capture right
        promotionMoves = moves & RANK_1; // Promotion on 1st rank
    }

    // Handle promotion moves by "promoting" to queen (for simplicity)
    moves |= promotionMoves; // Future extension: Handle knight, rook, bishop promotions
    return moves;
}

// Update en passant square if a pawn moves two squares
void updateEnPassantTarget(uint64_t moveFrom, uint64_t moveTo, bool isWhite) {
    enPassantTarget = 0;
    if (isWhite && (moveFrom & RANK_2) && (moveTo & RANK_4)) {
        enPassantTarget = moveTo << 8;
    } else if (!isWhite && (moveFrom & RANK_7) && (moveTo & RANK_5)) {
        enPassantTarget = moveTo >> 8;
    }
}

// Expanded function to check if a square is attacked by any enemy piece
bool isSquareAttacked(uint64_t square, bool byWhite) {
    uint64_t enemyPawns = byWhite ? whitePawns : blackPawns;
    uint64_t enemyKnights = byWhite ? whiteKnights : blackKnights;
    uint64_t enemyBishops = byWhite ? whiteBishops : blackBishops;
    uint64_t enemyRooks = byWhite ? whiteRooks : blackRooks;
    uint64_t enemyQueens = byWhite ? whiteQueens : blackQueens;
    uint64_t enemyKing = byWhite ? whiteKing : blackKing;

    // Pawn attacks
    if (byWhite) {
        if (((enemyPawns << 7) & ~FILE_H & square) || ((enemyPawns << 9) & ~FILE_A & square)) return true;
    } else {
        if (((enemyPawns >> 7) & ~FILE_A & square) || ((enemyPawns >> 9) & ~FILE_H & square)) return true;
    }

    // Knight attacks
    uint64_t knightAttacks = ((square << 17) & ~FILE_A) | ((square << 15) & ~FILE_H) |
                             ((square << 10) & ~(FILE_A | FILE_B)) | ((square << 6) & ~(FILE_G | FILE_H)) |
                             ((square >> 17) & ~FILE_H) | ((square >> 15) & ~FILE_A) |
                             ((square >> 10) & ~(FILE_G | FILE_H)) | ((square >> 6) & ~(FILE_A | FILE_B));
    if (enemyKnights & knightAttacks) return true;

    // Bishop and Queen (diagonal) attacks
    uint64_t diagonalMoves = slideMove(square, 9, allPieces & ~FILE_A) |
                             slideMove(square, 7, allPieces & ~FILE_H) |
                             slideMove(square, -9, allPieces & ~FILE_H) |
                             slideMove(square, -7, allPieces & ~FILE_A);
    if ((enemyBishops | enemyQueens) & diagonalMoves) return true;

    // Rook and Queen (straight) attacks
    uint64_t straightMoves = slideMove(square, 8, allPieces) | slideMove(square, -8, allPieces) |
                             slideMove(square, 1, allPieces & ~FILE_A) | slideMove(square, -1, allPieces & ~FILE_H);
    if ((enemyRooks | enemyQueens) & straightMoves) return true;

    // King attacks
    uint64_t kingAttacks = ((square << 8) | (square >> 8) | ((square & ~FILE_H) << 1) | ((square & ~FILE_A) >> 1) |
                            ((square & ~FILE_H) << 9) | ((square & ~FILE_A) << 7) |
                            ((square & ~FILE_H) >> 7) | ((square & ~FILE_A) >> 9));
    if (enemyKing & kingAttacks) return true;

    return false;
}

// Check if a move is legal by ensuring it doesn't leave the king in check
bool isMoveLegal(uint64_t fromSquare, uint64_t toSquare, bool isWhite, bool isEnPassant = false) {
    // Temporarily apply move
    uint64_t savedWhitePieces = whitePieces, savedBlackPieces = blackPieces, savedAllPieces = allPieces;
    if (isWhite) {
        whitePieces ^= fromSquare | toSquare;
        allPieces = whitePieces | blackPieces;
    } else {
        blackPieces ^= fromSquare | toSquare;
        allPieces = whitePieces | blackPieces;
    }

    // Check if king is in check after the move
    uint64_t king = isWhite ? whiteKing : blackKing;
    bool legal = !isSquareAttacked(king, !isWhite);

    // Restore original board state
    whitePieces = savedWhitePieces;
    blackPieces = savedBlackPieces;
    allPieces = savedAllPieces;

    return legal;
}
// Enhanced print function to display the board for players
void printBoardForPlayers() {
    cout << "\nCurrent Board:\n";
    cout << "  a b c d e f g h\n +----------------+\n";
    for (int rank = 7; rank >= 0; --rank) {
        cout << rank + 1 << "| ";
        for (int file = 0; file < 8; ++file) {
            int square = rank * 8 + file;
            uint64_t mask = 1ULL << square;
            if (whitePawns & mask) cout << "P ";
            else if (whiteKnights & mask) cout << "N ";
            else if (whiteBishops & mask) cout << "B ";
            else if (whiteRooks & mask) cout << "R ";
            else if (whiteQueens & mask) cout << "Q ";
            else if (whiteKing & mask) cout << "K ";
            else if (blackPawns & mask) cout << "p ";
            else if (blackKnights & mask) cout << "n ";
            else if (blackBishops & mask) cout << "b ";
            else if (blackRooks & mask) cout << "r ";
            else if (blackQueens & mask) cout << "q ";
            else if (blackKing & mask) cout << "k ";
            else cout << ". ";
        }
        cout << "|\n";
    }
    cout << " +----------------+\n";
}

// Parse move input like "e2 e4" to bitboard squares
pair<int, int> parseInput(const string& input) {
    int fileFrom = input[0] - 'a';
    int rankFrom = input[1] - '1';
    int fileTo = input[3] - 'a';
    int rankTo = input[4] - '1';

    int fromSquare = rankFrom * 8 + fileFrom;
    int toSquare = rankTo * 8 + fileTo;
    return {fromSquare, toSquare};
}

// Function to check for checkmate or stalemate
bool isCheckmateOrStalemate(bool isWhiteTurn) {
    uint64_t king = isWhiteTurn ? whiteKing : blackKing;
    bool kingInCheck = isSquareAttacked(king, !isWhiteTurn);

    // Iterate over all pieces of the current player and try all possible moves
    uint64_t pieces = isWhiteTurn ? whitePieces : blackPieces;
    while (pieces) {
        uint64_t piece = pieces & -pieces; // Isolate the least significant bit (current piece)
        pieces &= pieces - 1;              // Remove the isolated piece from the set

        // Try moving the piece to all possible destinations
        for (int destination = 0; destination < 64; ++destination) {
            uint64_t destinationBit = 1ULL << destination;
            if (destinationBit & allPieces) continue;  // Skip if occupied by same color
            if (!isMoveLegal(piece, destinationBit, isWhiteTurn)) continue;

            // If a legal move is found, not checkmate or stalemate
            return false;
        }
    }
    // Return true if the king is in check (checkmate), false if not (stalemate)
    return kingInCheck;
}

// Handle pawn promotion to a queen when reaching last rank
void handlePawnPromotion(uint64_t toBit, uint64_t isWhiteTurn) {
    if (isWhiteTurn && (toBit & RANK_8)) {
        whitePawns ^= toBit;
        whiteQueens |= toBit;
    }else if(!isWhiteTurn && (toBit & RANK_1)) {
        blackPawns ^= toBit;
        blackQueens |= toBit;
    }
    whitePieces = whitePawns | whiteKnights | whiteBishops | whiteRooks | whiteQueens | whiteKing;
    blackPieces = blackPawns | blackKnights | blackBishops | blackRooks | blackQueens | blackKing;
    allPieces = whitePieces | blackPieces;
}

// Make the move if it is valid
bool makeMove(int fromSquare, int toSquare, bool isWhiteTurn) {
    uint64_t fromBit = 1ULL << fromSquare;
    uint64_t toBit = 1ULL << toSquare;
    uint64_t pieceSet = isWhiteTurn ? whitePieces : blackPieces;

    if (pieceSet & fromBit) {
        if (isMoveLegal(fromBit, toBit, isWhiteTurn)) {
            if (isWhiteTurn) {
                whitePieces ^= fromBit | toBit;
                whitePawns &= ~fromBit;
                whiteKnights &= ~fromBit;
                whiteBishops &= ~fromBit;
                whiteRooks &= ~fromBit;
                whiteQueens &= ~fromBit;
                whitePawns |= toBit;  // Update appropriate piece sets
                handlePawnPromotion(toBit, isWhiteTurn);
            } else {
                blackPieces ^= fromBit | toBit;
                blackPawns &= ~fromBit;
                blackKnights &= ~fromBit;
                blackBishops &= ~fromBit;
                blackRooks &= ~fromBit;
                blackQueens &= ~fromBit;
                blackPawns |= toBit;  // Update appropriate piece sets
                handlePawnPromotion(toBit, isWhiteTurn);
            }
            allPieces = whitePieces | blackPieces;
            return true;
        }
    }
    return false;
}

// Main game loop
void gameLoop() {
    bool isWhiteTurn = true;
    initializePosition();
    printBoardForPlayers();

    while (true) {
        if (isCheckmateOrStalemate(isWhiteTurn)) {
            cout << (isWhiteTurn ? "Black wins by checkmate!\n" : "White wins by checkmate!\n");
            break;
        }

        cout << (isWhiteTurn ? "White's turn: " : "Black's turn: ");
        string moveInput;
        getline(cin, moveInput);

        if (moveInput.size() != 5 || moveInput[2] != ' ') {
            cout << "Invalid input format. Use format 'e2 e4'.\n";
            continue;
        }

        auto [fromSquare, toSquare] = parseInput(moveInput);
        if (makeMove(fromSquare, toSquare, isWhiteTurn)) {
            printBoardForPlayers();
            isWhiteTurn = !isWhiteTurn;
        } else {
            cout << "Invalid move. Try again.\n";
        }
    }
}

int main() {
    cout << "Welcome to Chess!\n";
    gameLoop();
    return 0;
}
