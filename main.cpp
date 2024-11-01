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

// Helper to print bitboards for testing (just for testing)
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

// Function to check if a move is legal
bool isMoveLegal(uint64_t fromSquare, uint64_t toSquare, bool isWhite) {
    uint64_t savedWhitePieces = whitePieces, savedBlackPieces = blackPieces, savedAllPieces = allPieces;
    uint64_t king = isWhite ? whiteKing : blackKing;

    // Make the move temporarily to check for king safety
    if (isWhite) {
        whitePieces ^= fromSquare | toSquare;
        allPieces = whitePieces | blackPieces;
    } else {
        blackPieces ^= fromSquare | toSquare;
        allPieces = whitePieces | blackPieces;
    }

    bool legal = !isSquareAttacked(king, !isWhite);

    // Restore pieces to original positions
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

vector<uint64_t> generatePawnMoves(uint64_t pawns, bool isWhite) {
    vector<uint64_t> moves;
    uint64_t singleStep, doubleStep, attacksLeft, attacksRight;

    if (isWhite) {
        // Single move forward if empty
        singleStep = (pawns << 8) & ~allPieces;

        // Double move forward only from rank 2 and if both squares are empty
        doubleStep = ((pawns & RANK_2) << 16) & ~allPieces & ~(allPieces << 8);

        // Capture moves: diagonal left and right
        attacksLeft = (pawns << 7) & blackPieces & ~FILE_H;  // Only if there's a black piece to the left
        attacksRight = (pawns << 9) & blackPieces & ~FILE_A; // Only if there's a black piece to the right
    } else {
        // Single move forward if empty
        singleStep = (pawns >> 8) & ~allPieces;

        // Double move forward only from rank 7 and if both squares are empty
        doubleStep = ((pawns & RANK_7) >> 16) & ~allPieces & ~(allPieces >> 8);

        // Capture moves: diagonal left and right
        attacksLeft = (pawns >> 7) & whitePieces & ~FILE_A;  // Only if there's a white piece to the left
        attacksRight = (pawns >> 9) & whitePieces & ~FILE_H; // Only if there's a white piece to the right
    }

    // Add moves to the list
    if (singleStep) moves.push_back(singleStep);
    if (doubleStep) moves.push_back(doubleStep);  // Only add double-step if valid
    if (attacksLeft) moves.push_back(attacksLeft);  // Only add if capture is possible
    if (attacksRight) moves.push_back(attacksRight); // Only add if capture is possible

    return moves;
}

// Generate knight moves
vector<uint64_t> generateKnightMoves(uint64_t knights, bool isWhite) {
    vector<uint64_t> moves;
    uint64_t targets = isWhite ? blackPieces : whitePieces;
    uint64_t potentialMoves;

    while (knights) {
        // Isolate the least significant knight piece
        uint64_t knight = knights & -knights;
        knights &= knights - 1;

        // Generate potential moves by shifting and masking
        potentialMoves = ((knight << 17) & ~FILE_A) | ((knight << 15) & ~FILE_H) |
                         ((knight << 10) & ~(FILE_A | FILE_B)) | ((knight << 6) & ~(FILE_G | FILE_H)) |
                         ((knight >> 17) & ~FILE_H) | ((knight >> 15) & ~FILE_A) |
                         ((knight >> 10) & ~(FILE_G | FILE_H)) | ((knight >> 6) & ~(FILE_A | FILE_B));

        // Remove own pieces from potential moves and add to moves list
        moves.push_back(potentialMoves & ~targets);
    }
    return moves;
}


// Generate bishop moves (diagonals)
vector<uint64_t> generateBishopMoves(uint64_t bishops, bool isWhite) {
    vector<uint64_t> moves;
    uint64_t targets = isWhite ? blackPieces : whitePieces;

    while (bishops) {
        uint64_t bishop = bishops & -bishops;
        bishops &= bishops - 1;
        uint64_t diagonalMoves = slideMove(bishop, 9, allPieces) | slideMove(bishop, 7, allPieces) |
                                 slideMove(bishop, -9, allPieces) | slideMove(bishop, -7, allPieces);
        moves.push_back(diagonalMoves & ~targets);
    }
    return moves;
}

// Generate rook moves (straight lines)
vector<uint64_t> generateRookMoves(uint64_t rooks, bool isWhite) {
    vector<uint64_t> moves;
    uint64_t targets = isWhite ? blackPieces : whitePieces;

    while (rooks) {
        uint64_t rook = rooks & -rooks;
        rooks &= rooks - 1;
        uint64_t straightMoves = slideMove(rook, 8, allPieces) | slideMove(rook, -8, allPieces) |
                                 slideMove(rook, 1, allPieces) | slideMove(rook, -1, allPieces);
        moves.push_back(straightMoves & ~targets);
    }
    return moves;
}

// Generate queen moves by combining rook and bishop moves
vector<uint64_t> generateQueenMoves(uint64_t queens, bool isWhite) {
    vector<uint64_t> moves;
    uint64_t targets = isWhite ? blackPieces : whitePieces;

    while (queens) {
        uint64_t queen = queens & -queens;
        queens &= queens - 1;
        uint64_t queenMoves = slideMove(queen, 8, allPieces) | slideMove(queen, -8, allPieces) |
                              slideMove(queen, 1, allPieces) | slideMove(queen, -1, allPieces) |
                              slideMove(queen, 9, allPieces) | slideMove(queen, 7, allPieces) |
                              slideMove(queen, -9, allPieces) | slideMove(queen, -7, allPieces);
        moves.push_back(queenMoves & ~targets);
    }
    return moves;
}

// Generate king moves
vector<uint64_t> generateKingMoves(uint64_t king, bool isWhite) {
    vector<uint64_t> moves;
    uint64_t targets = isWhite ? blackPieces : whitePieces;

    uint64_t kingMoves = ((king << 8) | (king >> 8) | ((king & ~FILE_H) << 1) | ((king & ~FILE_A) >> 1) |
                          ((king & ~FILE_H) << 9) | ((king & ~FILE_A) << 7) |
                          ((king & ~FILE_H) >> 7) | ((king & ~FILE_A) >> 9));
    moves.push_back(kingMoves & ~targets);

    return moves;
}

// Castling check
bool canCastleKingside(bool isWhite) {
    uint64_t kingPosition = isWhite ? whiteKing : blackKing;
    uint64_t rookPosition = isWhite ? whiteRooks : blackRooks;
    uint64_t kingsideMask = isWhite ? 0x60ULL : 0x6000000000000000ULL;

    // Ensure the squares between king and rook are empty, and check that the squares the king will move over are safe
    bool kingsideAvailable = (isWhite ? whiteKingsideCastle : blackKingsideCastle) &&
                             !(allPieces & kingsideMask) &&
                             !isSquareAttacked(kingPosition, !isWhite) &&
                             !isSquareAttacked(kingPosition << 1, !isWhite) &&
                             !isSquareAttacked(kingPosition << 2, !isWhite);
    return kingsideAvailable;
}

// Checks if the king can castle
bool canCastleQueenside(bool isWhite) {
    uint64_t kingPosition = isWhite ? whiteKing : blackKing;
    uint64_t rookPosition = isWhite ? whiteRooks : blackRooks;
    uint64_t queensideMask = isWhite ? 0xEULL : 0xE00000000000000ULL;

    bool queensideAvailable = (isWhite ? whiteQueensideCastle : blackQueensideCastle) &&
                              !(allPieces & queensideMask) &&
                              !isSquareAttacked(kingPosition, !isWhite) &&
                              !isSquareAttacked(kingPosition >> 1, !isWhite) &&
                              !isSquareAttacked(kingPosition >> 2, !isWhite);
    return queensideAvailable;
}

// En passant move generation
vector<uint64_t> generateEnPassantMoves(uint64_t pawns, bool isWhite) {
    vector<uint64_t> moves;
    if (enPassantTarget == 0) return moves;

    uint64_t enPassantLeft = isWhite ? (pawns << 7) & ~FILE_H & enPassantTarget
                                      : (pawns >> 7) & ~FILE_A & enPassantTarget;
    uint64_t enPassantRight = isWhite ? (pawns << 9) & ~FILE_A & enPassantTarget
                                       : (pawns >> 9) & ~FILE_H & enPassantTarget;

    if (enPassantLeft) moves.push_back(enPassantLeft);
    if (enPassantRight) moves.push_back(enPassantRight);
    return moves;
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

// Enhanced function to determine if a position is checkmate or stalemate
bool isCheckmateOrStalemate(bool isWhiteTurn) {
    uint64_t king = isWhiteTurn ? whiteKing : blackKing;
    bool kingInCheck = isSquareAttacked(king, !isWhiteTurn);

    // Iterate over all pieces of the current player and try all possible moves
    uint64_t pieces = isWhiteTurn ? whitePieces : blackPieces;
    while (pieces) {
        uint64_t piece = pieces & -pieces; // Isolate the least significant bit (current piece)
        pieces &= pieces - 1;              // Remove the isolated piece from the set

        // Generate all legal moves for this piece
        vector<uint64_t> legalMoves;
        if (isWhiteTurn) {
            if (whitePawns & piece) legalMoves = generatePawnMoves(piece, true);
            else if (whiteKnights & piece) legalMoves = generateKnightMoves(piece, true);
            else if (whiteBishops & piece) legalMoves = generateBishopMoves(piece, true);
            else if (whiteRooks & piece) legalMoves = generateRookMoves(piece, true);
            else if (whiteQueens & piece) legalMoves = generateQueenMoves(piece, true);
            else if (whiteKing & piece) legalMoves = generateKingMoves(piece, true);
        } else {
            if (blackPawns & piece) legalMoves = generatePawnMoves(piece, false);
            else if (blackKnights & piece) legalMoves = generateKnightMoves(piece, false);
            else if (blackBishops & piece) legalMoves = generateBishopMoves(piece, false);
            else if (blackRooks & piece) legalMoves = generateRookMoves(piece, false);
            else if (blackQueens & piece) legalMoves = generateQueenMoves(piece, false);
            else if (blackKing & piece) legalMoves = generateKingMoves(piece, false);
        }

        // Check each legal move to see if it leaves the king safe
        for (uint64_t move : legalMoves) {
            if (isMoveLegal(piece, move, isWhiteTurn)) {
                // If at least one legal move exists, it's not checkmate or stalemate
                return false;
            }
        }
    }

    // If no legal moves and king is in check, it's checkmate; otherwise, stalemate
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

// Main move function with updated castling and en passant checks
bool makeMove(int fromSquare, int toSquare, bool isWhiteTurn) {
    uint64_t fromBit = 1ULL << fromSquare;
    uint64_t toBit = 1ULL << toSquare;
    vector<uint64_t> legalMoves;

    // Check castling rights and update if applicable
    if (isWhiteTurn) {
        if ((whiteKing & fromBit) && (toSquare == fromSquare + 2) && canCastleKingside(true)) {
            whiteKing = toBit;
            whiteRooks ^= 0x80; // Update rook position for castling
            whiteKingsideCastle = false;
            whiteQueensideCastle = false;
            return true;
        } else if ((whiteKing & fromBit) && (toSquare == fromSquare - 2) && canCastleQueenside(true)) {
            whiteKing = toBit;
            whiteRooks ^= 0x1; // Update rook position for castling
            whiteKingsideCastle = false;
            whiteQueensideCastle = false;
            return true;
        }
    } else {
        if ((blackKing & fromBit) && (toSquare == fromSquare + 2) && canCastleKingside(false)) {
            blackKing = toBit;
            blackRooks ^= 0x8000000000000000; // Update rook position for castling
            blackKingsideCastle = false;
            blackQueensideCastle = false;
            return true;
        } else if ((blackKing & fromBit) && (toSquare == fromSquare - 2) && canCastleQueenside(false)) {
            blackKing = toBit;
            blackRooks ^= 0x100000000000000; // Update rook position for castling
            blackKingsideCastle = false;
            blackQueensideCastle = false;
            return true;
        }
    }

    // Generate legal moves for each piece type with en passant
    if (isWhiteTurn) {
        if (whitePawns & fromBit) {
            legalMoves = generatePawnMoves(fromBit, true);
            vector<uint64_t> enPassantMoves = generateEnPassantMoves(fromBit, true);
            legalMoves.insert(legalMoves.end(), enPassantMoves.begin(), enPassantMoves.end());
        } else if (whiteKnights & fromBit) legalMoves = generateKnightMoves(fromBit, true);
        else if (whiteBishops & fromBit) legalMoves = generateBishopMoves(fromBit, true);
        else if (whiteRooks & fromBit) legalMoves = generateRookMoves(fromBit, true);
        else if (whiteQueens & fromBit) legalMoves = generateQueenMoves(fromBit, true);
        else if (whiteKing & fromBit) legalMoves = generateKingMoves(fromBit, true);
    } else {
        if (blackPawns & fromBit) {
            legalMoves = generatePawnMoves(fromBit, false);
            vector<uint64_t> enPassantMoves = generateEnPassantMoves(fromBit, false);
            legalMoves.insert(legalMoves.end(), enPassantMoves.begin(), enPassantMoves.end());
        } else if (blackKnights & fromBit) legalMoves = generateKnightMoves(fromBit, false);
        else if (blackBishops & fromBit) legalMoves = generateBishopMoves(fromBit, false);
        else if (blackRooks & fromBit) legalMoves = generateRookMoves(fromBit, false);
        else if (blackQueens & fromBit) legalMoves = generateQueenMoves(fromBit, false);
        else if (blackKing & fromBit) legalMoves = generateKingMoves(fromBit, false);
    }

    // Check if the move is in the list of legal moves
    if (find(legalMoves.begin(), legalMoves.end(), toBit) == legalMoves.end()) {
        cout << "Illegal move for the piece. Try again.\n";
        return false;
    }

    // Check if move is legal regarding checks
    if (!isMoveLegal(fromBit, toBit, isWhiteTurn)) {
        cout << "Move leaves the king in check. Try again.\n";
        return false;
    }

    // Apply the move if legal
    if (isWhiteTurn) {
        whitePieces ^= fromBit | toBit;
        allPieces = whitePieces | blackPieces;

        // Update specific white piece bitboards
        if (whitePawns & fromBit) { whitePawns ^= fromBit | toBit; }
        else if (whiteKnights & fromBit) { whiteKnights ^= fromBit | toBit; }
        else if (whiteBishops & fromBit) { whiteBishops ^= fromBit | toBit; }
        else if (whiteRooks & fromBit) { whiteRooks ^= fromBit | toBit; }
        else if (whiteQueens & fromBit) { whiteQueens ^= fromBit | toBit; }
        else if (whiteKing & fromBit) { whiteKing ^= fromBit | toBit; }
    } else {
        blackPieces ^= fromBit | toBit;
        allPieces = whitePieces | blackPieces;

        // Update specific black piece bitboards
        if (blackPawns & fromBit) { blackPawns ^= fromBit | toBit; }
        else if (blackKnights & fromBit) { blackKnights ^= fromBit | toBit; }
        else if (blackBishops & fromBit) { blackBishops ^= fromBit | toBit; }
        else if (blackRooks & fromBit) { blackRooks ^= fromBit | toBit; }
        else if (blackQueens & fromBit) { blackQueens ^= fromBit | toBit; }
        else if (blackKing & fromBit) { blackKing ^= fromBit | toBit; }
    }

    // Handle en passant target square update
    if ((whitePawns & toBit) && abs(fromSquare - toSquare) == 16) {
        enPassantTarget = isWhiteTurn ? (1ULL << (toSquare - 8)) : (1ULL << (toSquare + 8));
    } else {
        enPassantTarget = 0;
    }

    return true;
}

// Evaluate the current position
int evaluatePosition() {
    // Piece values
    const int PAWN_VALUE = 100;
    const int KNIGHT_VALUE = 320;
    const int BISHOP_VALUE = 330;
    const int ROOK_VALUE = 500;
    const int QUEEN_VALUE = 900;
    const int KING_VALUE = 20000;

    // Calculate White's material score
    int whiteScore = 0;
    whiteScore += __builtin_popcountll(whitePawns) * PAWN_VALUE;
    whiteScore += __builtin_popcountll(whiteKnights) * KNIGHT_VALUE;
    whiteScore += __builtin_popcountll(whiteBishops) * BISHOP_VALUE;
    whiteScore += __builtin_popcountll(whiteRooks) * ROOK_VALUE;
    whiteScore += __builtin_popcountll(whiteQueens) * QUEEN_VALUE;
    whiteScore += __builtin_popcountll(whiteKing) * KING_VALUE;

    // Calculate Black's material score
    int blackScore = 0;
    blackScore += __builtin_popcountll(blackPawns) * PAWN_VALUE;
    blackScore += __builtin_popcountll(blackKnights) * KNIGHT_VALUE;
    blackScore += __builtin_popcountll(blackBishops) * BISHOP_VALUE;
    blackScore += __builtin_popcountll(blackRooks) * ROOK_VALUE;
    blackScore += __builtin_popcountll(blackQueens) * QUEEN_VALUE;
    blackScore += __builtin_popcountll(blackKing) * KING_VALUE;

    // Final evaluation: Positive score favors White, negative favors Black
    return whiteScore - blackScore;
}

// Main game loop
void gameLoop() {
    bool isWhiteTurn = true;
    initializePosition();
    printBoardForPlayers();

    while (true) {
        if (isCheckmateOrStalemate(isWhiteTurn)) {
            if (isSquareAttacked(isWhiteTurn ? whiteKing : blackKing, !isWhiteTurn)) {
                cout << (isWhiteTurn ? "Black wins by checkmate!" : "White wins by checkmate!") << endl;
            } else {
                cout << "Stalemate! The game is a draw." << endl;
            }
            break;  // End the game
        }

        // Display turn and take input
        cout << (isWhiteTurn ? "White's turn: " : "Black's turn: ");
        string moveInput;
        getline(cin, moveInput);

        // Validate input format
        if (moveInput.size() != 5 || moveInput[2] != ' ') {
            cout << "Invalid input format. Use format 'e2 e4'.\n";
            continue;
        }

        // Parse and make move
        auto [fromSquare, toSquare] = parseInput(moveInput);
        if (makeMove(fromSquare, toSquare, isWhiteTurn)) {
            printBoardForPlayers();
            int score = evaluatePosition();
            cout << "Evaluation Score: " << score << " ("
                 << (score > 0 ? "White is better" : (score < 0 ? "Black is better" : "Equal"))
                 << ")\n";
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