#ifndef POLYGLOT_BOOK_HPP
#define POLYGLOT_BOOK_HPP


#include "chess.hpp"
#include "polyglot.hpp"

#include <cstdint>
#include <cstddef>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <algorithm>


namespace polyglot
{


// ============================================================
// Polyglot book entry
//
// 16 bytes:
//
// key     uint64
// move    uint16
// weight  uint16
// learn   uint32
//
// Big endian
// ============================================================


struct BookEntry
{
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
};



// ============================================================
// endian read
// ============================================================


inline uint16_t read16(
    std::ifstream& f
)
{
    uint8_t b[2];

    f.read(
        reinterpret_cast<char*>(b),
        2
    );

    return
        (uint16_t(b[0]) << 8)
        |
        uint16_t(b[1]);
}



inline uint32_t read32(
    std::ifstream& f
)
{
    uint8_t b[4];

    f.read(
        reinterpret_cast<char*>(b),
        4
    );


    return
        (uint32_t(b[0]) << 24)
        |
        (uint32_t(b[1]) << 16)
        |
        (uint32_t(b[2]) << 8)
        |
        uint32_t(b[3]);
}



inline uint64_t read64(
    std::ifstream& f
)
{
    uint8_t b[8];

    f.read(
        reinterpret_cast<char*>(b),
        8
    );


    return
        (uint64_t(b[0]) << 56)
        |
        (uint64_t(b[1]) << 48)
        |
        (uint64_t(b[2]) << 40)
        |
        (uint64_t(b[3]) << 32)
        |
        (uint64_t(b[4]) << 24)
        |
        (uint64_t(b[5]) << 16)
        |
        (uint64_t(b[6]) << 8)
        |
        uint64_t(b[7]);
}



// ============================================================
// memory read
//
// 与上方 ifstream 版本逐位一致，
// 用于解析嵌入可执行文件的开局库字节
// ============================================================


inline uint16_t read16_mem(
    const uint8_t* p
)
{
    return
        (uint16_t(p[0]) << 8)
        |
        uint16_t(p[1]);
}



inline uint32_t read32_mem(
    const uint8_t* p
)
{
    return
        (uint32_t(p[0]) << 24)
        |
        (uint32_t(p[1]) << 16)
        |
        (uint32_t(p[2]) << 8)
        |
        uint32_t(p[3]);
}



inline uint64_t read64_mem(
    const uint8_t* p
)
{
    return
        (uint64_t(p[0]) << 56)
        |
        (uint64_t(p[1]) << 48)
        |
        (uint64_t(p[2]) << 40)
        |
        (uint64_t(p[3]) << 32)
        |
        (uint64_t(p[4]) << 24)
        |
        (uint64_t(p[5]) << 16)
        |
        (uint64_t(p[6]) << 8)
        |
        uint64_t(p[7]);
}



// ============================================================
// decoded move
// ============================================================


struct BookMove
{
    uint16_t raw;

    uint16_t weight;

    uint32_t learn;


    std::string uci;
};



// ============================================================
// square -> string
// ============================================================


inline std::string squareName(
    int sq
)
{
    std::string s;

    s += char('a' + (sq & 7));

    s += char('1' + (sq >> 3));

    return s;
}



// ============================================================
// promotion
// ============================================================


inline char promoChar(
    int p
)
{
    switch(p)
    {
    case 1:
        return 'n';

    case 2:
        return 'b';

    case 3:
        return 'r';

    case 4:
        return 'q';

    default:
        return 0;
    }
}



// ============================================================
// decode Polyglot move
// ============================================================


inline std::string decodeMove(
    uint16_t move
)
{
    int from =
        (move >> 6)
        & 63;


    int to =
        move
        & 63;


    int promo =
        (move >> 12)
        & 7;



    // =====================================================
    // Polyglot castle conversion
    //
    // Polyglot:
    // e1h1 -> UCI e1g1
    // e1a1 -> UCI e1c1
    // e8h8 -> UCI e8g8
    // e8a8 -> UCI e8c8
    // =====================================================

    if(from == 4)          // e1
    {
        if(to == 7)        // h1
            to = 6;        // g1

        else if(to == 0)   // a1
            to = 2;        // c1
    }
    else if(from == 60)    // e8
    {
        if(to == 63)       // h8
            to = 62;       // g8

        else if(to == 56)  // a8
            to = 58;       // c8
    }




    std::string s;


    s += squareName(from);

    s += squareName(to);



    if(promo)
    {
        s += promoChar(promo);
    }



    return s;
}



// ============================================================
// Polyglot book
// ============================================================


class Book
{

private:

    std::vector<BookEntry> entries;



public:


    bool load(
        const std::string& filename
    )
    {

        std::ifstream file(
            filename,
            std::ios::binary
        );


        if(!file)
            return false;



        entries.clear();



        while(file.peek()!=EOF)
        {

            BookEntry e;


            e.key =
                read64(file);


            e.move =
                read16(file);


            e.weight =
                read16(file);


            e.learn =
                read32(file);



            if(file)
                entries.push_back(e);
        }


        return true;
    }



    // --------------------------------------------------------
    // 从内存镜像加载（嵌入 EXE 的开局库）
    //
    // 逐条记录与 load() 完全一致；
    // 不足 16 字节的尾部残记录忽略
    // --------------------------------------------------------


    bool load_from_memory(
        const uint8_t* data,
        std::size_t bytes
    )
    {

        if(!data)
            return false;


        entries.clear();


        for(std::size_t off = 0; off + 16 <= bytes; off += 16)
        {

            const uint8_t* p = data + off;


            BookEntry e;


            e.key    = read64_mem(p);
            e.move   = read16_mem(p + 8);
            e.weight = read16_mem(p + 10);
            e.learn  = read32_mem(p + 12);


            entries.push_back(e);
        }


        return true;
    }



    size_t size() const
    {
        return entries.size();
    }



    // --------------------------------------------------------
    // find all moves
    // --------------------------------------------------------


    std::vector<BookMove> probe(
        uint64_t key
    ) const
    {

        std::vector<BookMove> result;



        for(const auto& e:entries)
        {

            if(e.key == key)
            {

                result.push_back(
                {
                    e.move,
                    e.weight,
                    e.learn,
                    decodeMove(e.move)
                });
            }
        }


        return result;
    }



    // --------------------------------------------------------
    // weighted random choice
    // --------------------------------------------------------


    BookMove choose(
        uint64_t key
    ) const
    {

        auto moves =
            probe(key);



        if(moves.empty())
            return {};



        uint32_t total=0;


        for(auto& m:moves)
            total += m.weight;



        std::random_device rd;

        std::mt19937 gen(rd());


        std::uniform_int_distribution<uint32_t>
        dist(
            1,
            total
        );



        uint32_t r =
            dist(gen);



        for(auto& m:moves)
        {

            if(r <= m.weight)
                return m;


            r -= m.weight;
        }


        return moves.back();
    }

};



}


#endif