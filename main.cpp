#include <iostream>
#include <bitset>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <limits>
#include <stack>
#include <random>
#include <unordered_map>

using namespace std;

// Board constants
const int BOARD_SIZE = 8;
const uint64_t FILE_A = 0x0101010101010101ULL;
const uint64_t FILE_B = 0x0202020202020202ULL;
const uint64_t FILE_G = 0x4040404040404040ULL;
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

stack<uint64_t> zobristHistory; // For undoing Zobrist hashes efficiently
uint64_t zobristTable[12][64];  // Randomized Zobrist keys for hashing
unordered_map<uint64_t, pair<int, int>> transpositionTable;

// Initialize Zobrist hashing
void initializeZobrist() {
    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

    for (int piece = 0; piece < 12; ++piece) {
        for (int square = 0; square < 64; ++square) {
            zobristTable[piece][square] = dist(gen);
        }
    }
}

// Initialize board position
void initializePosition() {
    whitePawns = 0x000000000000FF00ULL;
    whiteKnights = 0x0000000000000042ULL;
    whiteBishops = 0x0000000000000024ULL;
    whiteRooks = 0x0000000000000081ULL;
    whiteQueens = 0x0000000000000008ULL;
    whiteKing = 0x0000000000000010ULL;

    blackPawns = 0x00FF000000000000ULL;
    blackKnights = 0x4200000000000000ULL;
    blackBishops = 0x2400000000000000ULL;
    blackRooks = 0x8100000000000000ULL;
    blackQueens = 0x0800000000000000ULL;
    blackKing = 0x1000000000000000ULL;

    whitePieces = whitePawns | whiteKnights | whiteBishops | whiteRooks | whiteQueens | whiteKing;
    blackPieces = blackPawns | blackKnights | blackBishops | blackRooks | blackQueens | blackKing;
    allPieces = whitePieces | blackPieces;

    whiteKingsideCastle = whiteQueensideCastle = true;
    blackKingsideCastle = blackQueensideCastle = true;
    enPassantTarget = 0;

    // Initialize Zobrist hash for the initial position
    zobristHistory.push(0); // Push an initial Zobrist hash (to be calculated dynamically)
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
        if (temp & blockers) {  // Stop at blockers
            moves |= temp;      // Include the blocker square for capture
            break;
        }
        if ((temp & FILE_A) && (direction == -1 || direction == 7 || direction == -9)) break; // Prevent wrap from H to A
        if ((temp & FILE_H) && (direction == 1 || direction == -7 || direction == 9)) break;  // Prevent wrap from A to H
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
    uint64_t enemyPieces = byWhite ? whitePieces : blackPieces;

    // Pawn attacks
    if (byWhite) {
        if (((enemyPawns << 7) & ~FILE_H & square) || ((enemyPawns << 9) & ~FILE_A & square)) {
            return true; // White Pawn attacking left or right
        }
    } else {
        if (((enemyPawns >> 7) & ~FILE_A & square) || ((enemyPawns >> 9) & ~FILE_H & square)) {
            return true; // Black Pawn attacking left or right
        }
    }

    // Knight attacks
    uint64_t knightAttacks = ((square << 17) & ~FILE_A) | ((square << 15) & ~FILE_H) |
                             ((square >> 17) & ~FILE_H) | ((square >> 15) & ~FILE_A) |
                             ((square << 10) & ~(FILE_A | FILE_B)) | ((square >> 10) & ~(FILE_H | FILE_G)) |
                             ((square << 6) & ~(FILE_H | FILE_G)) | ((square >> 6) & ~(FILE_A | FILE_B));
    if (enemyKnights & knightAttacks) return true;

    // Sliding piece attacks (bishops, rooks, queens)
    uint64_t bishopAttacks = slideMove(square, 7, enemyPieces) | slideMove(square, 9, enemyPieces) |
                             slideMove(square, -7, enemyPieces) | slideMove(square, -9, enemyPieces);
    if (enemyBishops & bishopAttacks || enemyQueens & bishopAttacks) return true;

    uint64_t rookAttacks = slideMove(square, 1, enemyPieces) | slideMove(square, -1, enemyPieces) |
                           slideMove(square, 8, enemyPieces) | slideMove(square, -8, enemyPieces);
    if (enemyRooks & rookAttacks || enemyQueens & rookAttacks) return true;

    // King attacks
    uint64_t kingAttacks = (square << 8) | (square >> 8) |
                           ((square << 1) & ~FILE_A) | ((square >> 1) & ~FILE_H) |
                           ((square << 9) & ~FILE_A) | ((square >> 9) & ~FILE_H) |
                           ((square << 7) & ~FILE_H) | ((square >> 7) & ~FILE_A);
    if (enemyKing & kingAttacks) return true;

    return false;
}


// Convert a square in bitboard format to chess notation (e.g., 1ULL << 0 -> "a1")
string squareToNotation(uint64_t square) {
    if (square == 0) {
        return "null"; // Handle case where square is invalid
    }

    // Get the index of the square (position of the least significant bit)
    int index = __builtin_ctzll(square); // Count trailing zeros (find the bit position)

    // Convert the index into file (column) and rank (row)
    char file = 'a' + (index % 8);      // Files are a-h (0-7 mod 8)
    char rank = '1' + (index / 8);      // Ranks are 1-8 (0-7 div 8)

    return string(1, file) + string(1, rank); // Combine file and rank into a single string
}

// Function to check if a move is legal
bool isMoveLegal(uint64_t fromSquare, uint64_t toSquare, bool isWhite) {
    uint64_t savedWhitePieces = whitePieces;
    uint64_t savedBlackPieces = blackPieces;
    uint64_t savedAllPieces = allPieces;
    uint64_t king = isWhite ? whiteKing : blackKing;

    // Temporarily make the move
    if (isWhite) {
        whitePieces ^= fromSquare | toSquare;
        allPieces = whitePieces | blackPieces;
    } else {
        blackPieces ^= fromSquare | toSquare;
        allPieces = whitePieces | blackPieces;
    }

    // Check if the king is in check
    bool kingInCheck = isSquareAttacked(king, !isWhite);
    if (kingInCheck) {
        cout << "Move from " << squareToNotation(fromSquare) << " to " << squareToNotation(toSquare)
             << " leaves king in check. Illegal.\n";
    }

    // Restore original positions
    whitePieces = savedWhitePieces;
    blackPieces = savedBlackPieces;
    allPieces = savedAllPieces;

    return !kingInCheck;
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
        singleStep = (pawns << 8) & ~allPieces;
        doubleStep = ((pawns & RANK_2) << 16) & ~allPieces & ~(allPieces << 8);
        attacksLeft = (pawns << 7) & blackPieces & ~FILE_H;
        attacksRight = (pawns << 9) & blackPieces & ~FILE_A;
    } else {
        singleStep = (pawns >> 8) & ~allPieces;
        doubleStep = ((pawns & RANK_7) >> 16) & ~allPieces & ~(allPieces >> 8);
        attacksLeft = (pawns >> 7) & whitePieces & ~FILE_A;
        attacksRight = (pawns >> 9) & whitePieces & ~FILE_H;
    }

    // Debug output
    cout << "Pawn moves generated: ";
    if (singleStep) cout << singleStep << " ";
    if (doubleStep) cout << doubleStep << " ";
    if (attacksLeft) cout << attacksLeft << " ";
    if (attacksRight) cout << attacksRight << " ";
    cout << endl;

    if (singleStep) moves.push_back(singleStep);
    if (doubleStep) moves.push_back(doubleStep);
    if (attacksLeft) moves.push_back(attacksLeft);
    if (attacksRight) moves.push_back(attacksRight);

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

    // Debug move attempt
    cout << "Attempting move: " << squareToNotation(fromBit) << " -> " << squareToNotation(toBit) << "\n";

    // Generate legal moves for the piece
    vector<uint64_t> legalMoves;
    if (isWhiteTurn) {
        if (whitePawns & fromBit) legalMoves = generatePawnMoves(fromBit, true);
        else if (whiteKnights & fromBit) legalMoves = generateKnightMoves(fromBit, true);
        else if (whiteBishops & fromBit) legalMoves = generateBishopMoves(fromBit, true);
        else if (whiteRooks & fromBit) legalMoves = generateRookMoves(fromBit, true);
        else if (whiteQueens & fromBit) legalMoves = generateQueenMoves(fromBit, true);
        else if (whiteKing & fromBit) legalMoves = generateKingMoves(fromBit, true);
    } else {
        if (blackPawns & fromBit) legalMoves = generatePawnMoves(fromBit, false);
        else if (blackKnights & fromBit) legalMoves = generateKnightMoves(fromBit, false);
        else if (blackBishops & fromBit) legalMoves = generateBishopMoves(fromBit, false);
        else if (blackRooks & fromBit) legalMoves = generateRookMoves(fromBit, false);
        else if (blackQueens & fromBit) legalMoves = generateQueenMoves(fromBit, false);
        else if (blackKing & fromBit) legalMoves = generateKingMoves(fromBit, false);
    }

    // Debugging legal moves
    cout << "Legal moves for " << squareToNotation(fromBit) << ": ";
    for (uint64_t move : legalMoves) {
        cout << squareToNotation(move) << " ";
    }
    cout << endl;

    if (find(legalMoves.begin(), legalMoves.end(), toBit) == legalMoves.end()) {
        cout << "Move is not in the list of legal moves. Illegal.\n";
        return false;
    }

    // King safety check
    if (!isMoveLegal(fromBit, toBit, isWhiteTurn)) {
        cout << "Move leaves the king in check. Illegal.\n";
        return false;
    }

    // Apply the move
    cout << "Move is valid. Applying move.\n";
    if (isWhiteTurn) {
        whitePieces ^= fromBit | toBit;
        allPieces = whitePieces | blackPieces;

        if (whitePawns & fromBit) whitePawns ^= fromBit | toBit;
        else if (whiteKnights & fromBit) whiteKnights ^= fromBit | toBit;
        else if (whiteBishops & fromBit) whiteBishops ^= fromBit | toBit;
        else if (whiteRooks & fromBit) whiteRooks ^= fromBit | toBit;
        else if (whiteQueens & fromBit) whiteQueens ^= fromBit | toBit;
        else if (whiteKing & fromBit) whiteKing ^= fromBit | toBit;
    } else {
        blackPieces ^= fromBit | toBit;
        allPieces = whitePieces | blackPieces;

        if (blackPawns & fromBit) blackPawns ^= fromBit | toBit;
        else if (blackKnights & fromBit) blackKnights ^= fromBit | toBit;
        else if (blackBishops & fromBit) blackBishops ^= fromBit | toBit;
        else if (blackRooks & fromBit) blackRooks ^= fromBit | toBit;
        else if (blackQueens & fromBit) blackQueens ^= fromBit | toBit;
        else if (blackKing & fromBit) blackKing ^= fromBit | toBit;
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

    // Positional bonuses
    const int CENTER_CONTROL = 20; // Bonus for controlling central squares
    const uint64_t CENTER_MASK = 0x0000001818000000ULL;

    // Calculate material score
    int whiteScore = __builtin_popcountll(whitePawns) * PAWN_VALUE +
                     __builtin_popcountll(whiteKnights) * KNIGHT_VALUE +
                     __builtin_popcountll(whiteBishops) * BISHOP_VALUE +
                     __builtin_popcountll(whiteRooks) * ROOK_VALUE +
                     __builtin_popcountll(whiteQueens) * QUEEN_VALUE +
                     __builtin_popcountll(whiteKing) * KING_VALUE;

    int blackScore = __builtin_popcountll(blackPawns) * PAWN_VALUE +
                     __builtin_popcountll(blackKnights) * KNIGHT_VALUE +
                     __builtin_popcountll(blackBishops) * BISHOP_VALUE +
                     __builtin_popcountll(blackRooks) * ROOK_VALUE +
                     __builtin_popcountll(blackQueens) * QUEEN_VALUE +
                     __builtin_popcountll(blackKing) * KING_VALUE;

    // Add bonuses for center control
    whiteScore += __builtin_popcountll(whitePieces & CENTER_MASK) * CENTER_CONTROL;
    blackScore += __builtin_popcountll(blackPieces & CENTER_MASK) * CENTER_CONTROL;

    // Return evaluation
    return whiteScore - blackScore;
}



// Define a structure to hold board state information
struct BoardState {
    uint64_t whitePawns, whiteKnights, whiteBishops, whiteRooks, whiteQueens, whiteKing;
    uint64_t blackPawns, blackKnights, blackBishops, blackRooks, blackQueens, blackKing;
    uint64_t whitePieces, blackPieces, allPieces;
    uint64_t enPassantTarget;
    bool whiteKingsideCastle, whiteQueensideCastle;
    bool blackKingsideCastle, blackQueensideCastle;
    bool isWhiteTurn;
};



void cachePosition(uint64_t zobristHash, int evaluation, int depth) {
    transpositionTable[zobristHash] = make_pair(depth, evaluation);
}

int lookupTransposition(uint64_t zobristHash) {
    if (transpositionTable.count(zobristHash)) {
        return transpositionTable[zobristHash].second; // Return the evaluation part
    }
    return std::numeric_limits<int>::min();
}


// Stack to store previous board states
std::stack<BoardState> historyStack;

// Function to save the current board state before making a move
void saveBoardState(bool isWhiteTurn) {
    BoardState currentState = {
        whitePawns, whiteKnights, whiteBishops, whiteRooks, whiteQueens, whiteKing,
        blackPawns, blackKnights, blackBishops, blackRooks, blackQueens, blackKing,
        whitePieces, blackPieces, allPieces,
        enPassantTarget,
        whiteKingsideCastle, whiteQueensideCastle,
        blackKingsideCastle, blackQueensideCastle,
        isWhiteTurn // Capture the turn information
    };
    historyStack.push(currentState);
}


// Function to undo the last move by restoring the previous board state
void undoMove() {
    if (!historyStack.empty()) {
        BoardState lastState = historyStack.top();
        historyStack.pop();

        // Restore board pieces and other state information
        whitePawns = lastState.whitePawns;
        whiteKnights = lastState.whiteKnights;
        whiteBishops = lastState.whiteBishops;
        whiteRooks = lastState.whiteRooks;
        whiteQueens = lastState.whiteQueens;
        whiteKing = lastState.whiteKing;

        blackPawns = lastState.blackPawns;
        blackKnights = lastState.blackKnights;
        blackBishops = lastState.blackBishops;
        blackRooks = lastState.blackRooks;
        blackQueens = lastState.blackQueens;
        blackKing = lastState.blackKing;

        whitePieces = lastState.whitePieces;
        blackPieces = lastState.blackPieces;
        allPieces = lastState.allPieces;

        whiteKingsideCastle = lastState.whiteKingsideCastle;
        whiteQueensideCastle = lastState.whiteQueensideCastle;
        blackKingsideCastle = lastState.blackKingsideCastle;
        blackQueensideCastle = lastState.blackQueensideCastle;

        enPassantTarget = lastState.enPassantTarget;
    }
}


// Define a function to get the maximum evaluation
int max(int a, int b) {
    return (a > b) ? a : b;
}

// Define a function to get the minimum evaluation
int min(int a, int b) {
    return (a < b) ? a : b;
}
bool isCapture(uint64_t move, bool isWhiteTurn) {
    uint64_t targetSquare = move; // Assume `move` encodes the target square
    uint64_t opponentPieces = isWhiteTurn ? blackPieces : whitePieces;
    return targetSquare & opponentPieces;
}

bool isCheck(uint64_t move, bool isWhiteTurn) {
    saveBoardState(isWhiteTurn);
    makeMove(move, move, isWhiteTurn);

    uint64_t king = isWhiteTurn ? blackKing : whiteKing;
    bool result = isSquareAttacked(king, !isWhiteTurn);

    undoMove(); // Revert to the original state
    return result;
}

int movePriority(uint64_t move, bool isWhiteTurn) {
    int priority = 0;
    if (isCapture(move, isWhiteTurn)) {
        priority += 100; // High priority for captures
    }
    if (isCheck(move, isWhiteTurn)) {
        priority += 50; // Moderate priority for checks
    }
    return priority;
}

// Recursive minimax function with alpha-beta pruning
int minimax(int depth, bool isMaximizingPlayer, int alpha, int beta, bool isWhiteTurn) {
    uint64_t zobristHash = zobristHistory.top(); // Retrieve current Zobrist hash

    // Check transposition table
    if (transpositionTable.count(zobristHash)) {
        auto [storedDepth, storedEval] = transpositionTable[zobristHash];
        if (storedDepth >= depth) {
            return storedEval; // Use cached evaluation
        }
    }

    // Base case: if depth is 0 or the game is over
    if (depth == 0 || isCheckmateOrStalemate(isWhiteTurn)) {
        return evaluatePosition();
    }

    vector<uint64_t> legalMoves;
    int bestEval = isMaximizingPlayer ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();

    // Generate all moves for the current player
    uint64_t pieces = isWhiteTurn ? whitePieces : blackPieces;
    while (pieces) {
        uint64_t piece = pieces & -pieces; // Isolate least significant bit
        pieces &= pieces - 1;

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

        // Move ordering: prioritize captures or checks
        sort(legalMoves.begin(), legalMoves.end(), [isWhiteTurn](uint64_t a, uint64_t b) {
            return movePriority(a, isWhiteTurn) > movePriority(b, isWhiteTurn);
        });


        for (uint64_t move : legalMoves) {
            if (!isMoveLegal(piece, move, isWhiteTurn)) continue;

            saveBoardState(isWhiteTurn);
            makeMove(piece, move, isWhiteTurn);

            int eval;
            if (isMaximizingPlayer) {
                eval = minimax(depth - 1, false, alpha, beta, !isWhiteTurn);
                bestEval = max(bestEval, eval);
                alpha = max(alpha, eval);
            } else {
                eval = minimax(depth - 1, true, alpha, beta, !isWhiteTurn);
                bestEval = min(bestEval, eval);
                beta = min(beta, eval);
            }

            undoMove();

            if (beta <= alpha) {
                break; // Alpha-beta cutoff
            }
        }
    }

    // Store result in transposition table
    transpositionTable[zobristHash] = make_pair(depth, bestEval);


    return bestEval;
}

struct Move {
    uint64_t from;
    uint64_t to;
    int evaluation;
};

// Function to find the best move for the computer
Move findBestMove(bool isWhiteTurn, int depth = 4) {
    Move bestMove = {0, 0, isWhiteTurn ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max()};
    uint64_t pieces = isWhiteTurn ? whitePieces : blackPieces;

    while (pieces) {
        uint64_t piece = pieces & -pieces;
        pieces &= pieces - 1;

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

        for (uint64_t move : legalMoves) {
            if (!isMoveLegal(piece, move, isWhiteTurn)) continue;

            saveBoardState(isWhiteTurn);
            makeMove(piece, move, isWhiteTurn);

            int eval = minimax(depth - 1, !isWhiteTurn, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), !isWhiteTurn);
            undoMove();

            cout << "Move from " << squareToNotation(piece) << " to " << squareToNotation(move)
                 << " evaluated at " << eval << endl;

            if ((isWhiteTurn && eval > bestMove.evaluation) || (!isWhiteTurn && eval < bestMove.evaluation)) {
                bestMove = {piece, move, eval};
            }
        }
    }

    cout << "Best move selected: from " << squareToNotation(bestMove.from) << " to " << squareToNotation(bestMove.to)
         << " with evaluation " << bestMove.evaluation << endl;
    return bestMove;
}


// Game loop for playing against the computer
void computerGameLoop(bool humanPlaysWhite) {
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
            break;
        }

        if ((isWhiteTurn && humanPlaysWhite) || (!isWhiteTurn && !humanPlaysWhite)) {
            // Human move
            cout << (isWhiteTurn ? "White's turn: " : "Black's turn: ");
            string moveInput;
            getline(cin, moveInput);

            if (moveInput.size() != 5 || moveInput[2] != ' ') {
                cout << "Invalid input format. Use format 'e2 e4'.\n";
                continue;
            }

            pair<int, int> parsedMove = parseInput(moveInput);
            int fromSquare = parsedMove.first;
            int toSquare = parsedMove.second;

            if (!makeMove(fromSquare, toSquare, isWhiteTurn)) {
                cout << "Invalid move. Try again.\n";
                continue;
            }
        } else {
            // Computer move
            cout << "Computer is thinking...\n";
            Move bestMove = findBestMove(isWhiteTurn);
            if (bestMove.from == 0 && bestMove.to == 0) {
                cout << "No legal moves available for AI. Game over.\n";
                break;
            }
            makeMove(bestMove.from, bestMove.to, isWhiteTurn);
            cout << "Computer's move: Evaluation = " << bestMove.evaluation << endl;
        }

        printBoardForPlayers();
        isWhiteTurn = !isWhiteTurn;
    }
}


// Game loop for human vs. human gameplay
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
            break;
        }

        // Display turn and take input
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



// Main function to choose game mode
int main() {
    initializeZobrist();
    initializePosition();
    printBitboard(whitePawns);
    cout << "Welcome to Chess!\nChoose game mode:\n1. Human vs Human\n2. Human vs Computer\n";
    int choice;
    cin >> choice;
    cin.ignore(); // To ignore the newline character left in the input buffer

    if (choice == 1) {
        gameLoop();
    } else if (choice == 2) {
        cout << "Do you want to play as White? (y/n): ";
        char colorChoice;
        cin >> colorChoice;
        cin.ignore();
        bool humanPlaysWhite = (colorChoice == 'y' || colorChoice == 'Y');
        computerGameLoop(humanPlaysWhite);
    } else {
        cout << "Invalid choice. Exiting program.\n";
    }

    return 0;
}