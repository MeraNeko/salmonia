// evaluate.hpp
// v3.04a
#ifndef EVALUATE_HPP
#define EVALUATE_HPP

#include "chess.hpp"
#include <array>

namespace search {

// Material values in centipawns
constexpr int pieceValue[6] = {
     86,   // PAWN
    297,   // KNIGHT
    317,   // BISHOP
    471,   // ROOK
    917,   // QUEEN
    60000  // KING
};

constexpr int mobilityValue[6] = {
    29,     // PAWN
    62,     // KNIGHT
    50,     // BISHOP
    41,     // ROOK
    8,     // QUEEN
    0      // KING
};

// Piece-square tables (white perspective)
constexpr std::array<int, 64> PST_PAWN = {
    0,   0,   0,   0,   0,   0,   0,   0,
    78,  83,  86,  73, 102,  82,  85,  90,
    7,  29,  21,  44,  40,  31,  44,   7,
   -17,  16,  -2,  15,  14,   0,  15, -13,
   -26,   3,  10,   9,   6,   1,   0, -23,
   -22,   9,   5, -11, -10,  -2,   3, -19,
   -31,   8,  -7, -37, -36, -14,   3, -31,
    0,   0,   0,   0,   0,   0,   0,   0
};

constexpr std::array<int, 64> PST_KNIGHT = {
   -66, -53, -75, -75, -10, -55, -58, -70,
    -3,  -6, 100, -36,   4,  62,  -4, -14,
    10,  67,   1,  74,  73,  27,  62,  -2,
    24,  24,  45,  37,  33,  41,  25,  17,
    -1,   5,  31,  21,  22,  35,   2,   0,
   -18,  10,  13,  22,  18,  15,  11, -14,
   -23, -15,   2,   0,   2,   0, -23, -20,
   -74, -23, -26, -24, -19, -35, -22, -69
};

constexpr std::array<int, 64> PST_BISHOP = {
   -59, -78, -82, -76, -23,-107, -37, -50,
   -11,  20,  35, -42, -39,  31,   2, -22,
    -9,  39, -32,  41,  52, -10,  28, -14,
    25,  17,  20,  34,  26,  25,  15,  10,
    13,  10,  17,  23,  17,  16,   0,   7,
    14,  25,  24,  15,   8,  25,  20,  15,
    19,  20,  11,   6,   7,   6,  20,  16,
    -7,   2, -15, -12, -14, -15, -10, -10
};

constexpr std::array<int, 64> PST_ROOK = {
    35,  29,  33,   4,  37,  33,  56,  50,
    55,  29,  56,  67,  55,  62,  34,  60,
    19,  35,  28,  33,  45,  27,  25,  15,
     0,   5,  16,  13,  18,  -4,  -9,  -6,
   -28, -35, -16, -21, -13, -29, -46, -30,
   -42, -28, -42, -25, -25, -35, -26, -46,
   -53, -38, -31, -26, -29, -43, -44, -53,
   -30, -24, -18,   5,  -2, -18, -31, -32
};

constexpr std::array<int, 64> PST_QUEEN = {
     6,   1,  -8,-104,  69,  24,  88,  26,
    14,  32,  60, -10,  20,  76,  57,  24,
    -2,  43,  32,  60,  72,  63,  43,   2,
     1, -16,  22,  17,  25,  20, -13,  -6,
   -14, -15,  -2,  -5,  -1, -10, -20, -22,
   -30,  -6, -13, -11, -16, -11, -16, -27,
   -36, -18,   0, -19, -15, -15, -21, -38,
   -39, -30, -31, -13, -31, -36, -34, -42
};

constexpr std::array<int, 64> PST_KING = {
     4,  54,  47, -99, -99,  60,  83, -62,
   -32,  10,  55,  56,  56,  55,  10,   3,
   -62,  12, -57,  44, -67,  28,  37, -31,
   -55,  50,  11,  -4, -19,  13,   0, -49,
   -55, -43, -52, -28, -51, -47,  -8, -50,
   -47, -42, -43, -79, -64, -32, -29, -32,
    -4,   3, -14, -50, -57, -18,  13,   4,
    17,  30,  -3, -14,   6,  -1,  40,  18
};

constexpr int PST_KING_OP[] = {
   -41,     -52,     -36,     -111,     -108,     -116,     -127,     -142,                                                     
    -52,     -119,     -158,     -101,     -98,     -97,     -160,     -135,
    -49,     -80,     -116,     -125,     -106,     -86,     -59,     -66,
    -53,     -128,     -151,     -41,     -68,     -153,     -134,     -162,
    -98,     -94,     -45,     -190,     -90,     -96,     -79,     -118,
    -75,     -153,     -144,     -82,     -36,     -114,     -45,     -123,
    -126,     -82,     -114,     -74,     -83,     -25,     -59,     -51,
    61,     12,     -162,     -170,     -110,     -132,     67,     -41
};

constexpr int PST_PAWN_ED[64]=
{
     0,   0,   0,   0,   0,   0,   0,   0,
    70,  80,  85,  90,  90,  85,  80,  70,
    55,  60,  65,  70,  70,  65,  60,  55, 
    35,  40,  45,  50,  50,  45,  40,  35,
    15,  20,  25,  30,  30,  25,  20,  15,
   -10, -10,  -5,   0,   0,  -5, -10, -10,
   -30, -30, -30, -20, -20, -30, -30, -30,
     0,   0,   0,   0,   0,   0,   0,   0
};

constexpr std::array<std::array<int, 64>, 6> PST = {
    PST_PAWN,
    PST_KNIGHT,
    PST_BISHOP,
    PST_ROOK,
    PST_QUEEN,
    PST_KING,
};

inline int kingDistance(
    chess::Square a,
    chess::Square b
)
{
    int dx =
        abs(
            int(a.file())
            -
            int(b.file())
        );


    int dy =
        abs(
            int(a.rank())
            -
            int(b.rank())
        );


    return std::max(dx,dy);
}

/**
 * Static evaluation of a board position.
 * Returns a score from White's perspective (positive = better for White).
 */
inline int evaluate(const chess::Board& board) {
    int score = 0;
    int white_mobility = 0;
    int black_mobility = 0;

    int totalPieces = 0;               // total number of pieces on board
    int whiteNonPawnKing = 0;          // white pieces excluding pawns and king
    int pawn_op = 0;

    chess::Bitboard occ = board.occ();
    chess::Bitboard white_occ = board.us(chess::Color::WHITE);
    chess::Bitboard black_occ = board.us(chess::Color::BLACK);

    chess::Bitboard whiteAttackBB = 0;
    chess::Bitboard blackAttackBB = 0;

    while (occ) {
        chess::Square sq = occ.pop();
        chess::Piece piece = board.at(sq);
        chess::PieceType type = piece.type();
        int typeIdx = static_cast<int>(type);
        int sqIdx = sq.index();

        totalPieces++;

        if (piece.color() == chess::Color::WHITE &&
            type != chess::PieceType::PAWN &&
            type != chess::PieceType::KING) {
            whiteNonPawnKing++;
        }

        // Material and positional score
        if (piece.color() == chess::Color::WHITE) {
            score += pieceValue[typeIdx] + PST[typeIdx][sqIdx];
        } else {
            score -= pieceValue[typeIdx];
            score -= PST[typeIdx][sqIdx ^ 56];
        }

        // Mobility: count attacked squares (excluding own pieces)
        chess::Bitboard atks;
        switch (typeIdx) {
            case 0: // PAWN
                atks = chess::attacks::pawn(piece.color(), sq);
                if (piece.color() == chess::Color::WHITE) {
                    pawn_op += PST_PAWN_ED[sq.index()];
                } else {
                    pawn_op -= PST_PAWN_ED[sq.index() ^ 56];
                }
                break;
            case 1: // KNIGHT
                atks = chess::attacks::knight(sq);
                break;
            case 2: // BISHOP
                atks = chess::attacks::bishop(sq, occ);
                break;
            case 3: // ROOK
                atks = chess::attacks::rook(sq, occ);
                break;
            case 4: // QUEEN
                atks = chess::attacks::queen(sq, occ);
                break;
            case 5: // KING
                atks = chess::attacks::king(sq);
                break;
            default:
                atks = 0;
                break;
        }

        // Remove attacks on own pieces (cannot attack own pieces)
        if (piece.color() == chess::Color::WHITE) {
            atks &= ~white_occ;
            white_mobility += (atks.count() * mobilityValue[typeIdx]);
            whiteAttackBB |= atks;
        } else {
            atks &= ~black_occ;
            black_mobility += (atks.count() * mobilityValue[typeIdx]);
            blackAttackBB |= atks;
        }
    }

    
    // Opening: 0, Middle Game: 1, End Game: 2

    int game_step = 0;
    if (totalPieces < 7) {
        game_step = 2;
    } 
    else if (whiteNonPawnKing < 6) {
        game_step = 1;
    }

    
    // Mobility term: white attack count minus black attack count
    int mobility_weight = ((game_step == 0) ? 34 
                        : ((game_step == 1) ? 35 : 11));
    score += ((white_mobility - black_mobility) * mobility_weight) / 100;

    // Room for attack term: count of squares attacked by each side
    score += (whiteAttackBB.count()
            - blackAttackBB.count())/2;

    // King safety term: count of squares attacked by opponent's pieces around the king
    
    auto white_zone =
    chess::attacks::king(
        board.kingSq(chess::Color::WHITE)
    );
    auto black_zone =
    chess::attacks::king(
        board.kingSq(chess::Color::BLACK)
    );
    white_zone &= (blackAttackBB);
    
    black_zone &= (whiteAttackBB);

    int safety_weight = ((game_step == 0) ? 5 
                      : ((game_step == 1) ? 5 : 0));

    score -= white_zone.count() * safety_weight;
    score += black_zone.count() * safety_weight;

    if (game_step == 0) {
        score += PST_KING_OP[board.kingSq(chess::Color::WHITE).index()];
        score -= PST_KING_OP[board.kingSq(chess::Color::BLACK).index() ^ 56];
    }

    if (game_step == 2) {
        score += pawn_op;
    }

    if(game_step==2)
{

    chess::Square wk =
        board.kingSq(
            chess::Color::WHITE
        );


    chess::Square bk =
        board.kingSq(
            chess::Color::BLACK
        );



    // -----------------------------
    // 1. 自己王靠近对方王
    // -----------------------------


    int dist =
        kingDistance(
            wk,
            bk
        );


    // 越近越好
    score +=
        (14-dist) * 4;



    // -----------------------------
    // 2. 赶敌王到边角
    // -----------------------------


    auto edgeDistance =
    [](chess::Square sq)
    {

        int f =
            sq.index() & 7;

        int r =
            sq.index() >> 3;


        int df =
            std::min(f,7-f);

        int dr =
            std::min(r,7-r);


        return df+dr;

    };



    int enemyEdge =
        edgeDistance(bk);



    // 黑王越靠边越好
    score +=
        (6-enemyEdge)*10;



    // -----------------------------
    // 3. 白王中心化
    // -----------------------------


    int center =
            3 -
            std::max(
                abs((wk.index()&7)-3),
                abs((wk.index()>>3)-3)
            );


        score +=
            center*8;

    }

    return score;
}

} // namespace search

#endif // EVALUATE_HPP