#ifndef POLYGLOT_HPP
#define POLYGLOT_HPP


#include "chess.hpp"

#include <cstdint>


namespace polyglot
{


// ============================================================
// 781 Polyglot random numbers
// ============================================================

static constexpr uint64_t RANDOM[781] =
{
#include "polyglot_random.inc"
};



// ============================================================
// Square conversion
//
// Polyglot:
// a1 = 0
// b1 = 1
// ...
// h8 = 63
// ============================================================


inline int polySquare(
    chess::Square sq
)
{
    int index = sq.index();

    int file = index & 7;
    int rank = index >> 3;


    // chess.hpp:
    // a8=0 ... h1=63
    //
    // Polyglot:
    // a1=0 ... h8=63

    return (7-rank)*8 + file;
}





// ============================================================
// Piece mapping
//
// Polyglot order:
//
// 0 white pawn
// 1 white knight
// 2 white bishop
// 3 white rook
// 4 white queen
// 5 white king
//
// 6 black pawn
// ...
//
// ============================================================


inline int pieceIndex(chess::Piece piece)
{
    if(piece == chess::Piece::NONE)
        return -1;


    int color =
        (piece.color() == chess::Color::WHITE ? 1 : 0);


    int type;


    switch(piece.type())
    {
    case static_cast<int>(chess::PieceType::PAWN):
        type = 0;
        break;

    case static_cast<int>(chess::PieceType::KNIGHT):
        type = 1;
        break;

    case static_cast<int>(chess::PieceType::BISHOP):
        type = 2;
        break;

    case static_cast<int>(chess::PieceType::ROOK):
        type = 3;
        break;

    case static_cast<int>(chess::PieceType::QUEEN):
        type = 4;
        break;

    case static_cast<int>(chess::PieceType::KING):
        type = 5;
        break;

    default:
        return -1;
    }


    return type * 2 + color;
}





// ============================================================
// Polyglot hash
// ============================================================


inline uint64_t hash(const chess::Board& board)
{
    uint64_t key = 0;


    // pieces
    for(int sq=0;sq<64;sq++)
    {
        chess::Square square(sq);

        chess::Piece pc = board.at(square);

        int p = pieceIndex(pc);

        if(p >= 0)
        {
            uint64_t old = key;

            key ^= RANDOM[p*64 + sq];

        }
    }



    auto rights = board.castlingRights();


    if(rights.has(
        chess::Color::WHITE,
        chess::Board::CastlingRights::Side::KING_SIDE))
    {
        key ^= RANDOM[768];
    }

    if(rights.has(
        chess::Color::WHITE,
        chess::Board::CastlingRights::Side::QUEEN_SIDE))
    {
        key ^= RANDOM[769];
    }

    if(rights.has(
        chess::Color::BLACK,
        chess::Board::CastlingRights::Side::KING_SIDE))
    {
        key ^= RANDOM[770];
    }

    if(rights.has(
        chess::Color::BLACK,
        chess::Board::CastlingRights::Side::QUEEN_SIDE))
    {
        key ^= RANDOM[771];
    }



    auto ep = board.enpassantSq();


    if(ep != chess::Square::NO_SQ)
    {
        key ^= RANDOM[772 + ep.file()];
    }



    if(board.sideToMove()==chess::Color::WHITE)
    {
        key ^= RANDOM[780];
    }



    return key;
}



}



#endif