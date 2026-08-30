#ifndef TRANSPOSITION_HPP
#define TRANSPOSITION_HPP

#include "chess.hpp"

#include <cstdint>
#include <vector>


namespace search
{


enum class TTFlag : uint8_t
{
    EXACT,
    LOWERBOUND,
    UPPERBOUND
};



struct TTEntry
{
    uint64_t key = 0;

    int score = 0;

    int depth = -1;

    TTFlag flag = TTFlag::EXACT;

    chess::Move move{};
};



struct TTBucket
{
    TTEntry entry[2];
};




class TranspositionTable
{

private:

    std::vector<TTBucket> table;

    size_t mask = 0;



public:


    explicit TranspositionTable(size_t mb = 64)
    {
        resize(mb);
    }



    void resize(size_t mb)
    {

        size_t bytes =
            mb * 1024ULL * 1024ULL;


        size_t count =
            bytes / sizeof(TTBucket);



        size_t size = 1;


        while(size * 2 <= count)
            size *= 2;



        table.clear();

        table.resize(size);

        mask = size - 1;

    }





    void clear()
    {

        for(auto& b:table)
        {
            b.entry[0] = TTEntry{};
            b.entry[1] = TTEntry{};
        }

    }







    bool probe(
        const chess::Board& board,
        int depth,
        int alpha,
        int beta,
        int& score,
        chess::Move& ttMove,
        int ply = 0
    ) const
    {

        uint64_t key =
            board.hash();



        const auto& bucket =
            table[key & mask];




        for(int i=0;i<2;i++)
        {

            const TTEntry& e =
                bucket.entry[i];



            if(e.key != key)
                continue;



            /*
                完全信任 TT：
                TT move 仅用于走法排序，
                非法着法不在生成列表中、
                永远不会被实际搜索，
                无需在此做全量合法性校验
            */

            if(e.move != chess::Move{})
            {
                ttMove = e.move;
            }



            if(e.depth < depth)
                continue;



            int value = e.score;



            if(e.flag == TTFlag::EXACT)
            {
                score=value;
                return true;
            }


            if(e.flag == TTFlag::LOWERBOUND &&
               value >= beta)
            {
                score=value;
                return true;
            }



            if(e.flag == TTFlag::UPPERBOUND &&
               value <= alpha)
            {
                score=value;
                return true;
            }


        }



        return false;

    }









    chess::Move getMove(
        const chess::Board& board
    ) const
    {


        uint64_t key =
            board.hash();



        const auto& bucket =
            table[key & mask];



        for(int i=0;i<2;i++)
        {

            const TTEntry& e =
                bucket.entry[i];


            if(e.key == key)
            {

                if(e.move != chess::Move{})
                    return e.move;

            }

        }



        return chess::Move{};

    }









    void store(
        const chess::Board& board,
        int depth,
        int score,
        TTFlag flag,
        chess::Move move,
        int ply=0
    )
    {


        uint64_t key =
            board.hash();



        TTBucket& bucket =
            table[key & mask];



        TTEntry* replace=nullptr;



        // 1.覆盖同key

        for(auto& e:bucket.entry)
        {

            if(e.key==key)
            {
                replace=&e;
                break;
            }

        }





        // 2.找空槽

        if(!replace)
        {

            for(auto& e:bucket.entry)
            {

                if(e.key==0)
                {
                    replace=&e;
                    break;
                }

            }

        }





        // 3.替换浅节点

        if(!replace)
        {

            replace=&bucket.entry[0];


            if(bucket.entry[1].depth <
               replace->depth)
            {
                replace=&bucket.entry[1];
            }

        }







        replace->key=key;

        replace->depth=depth;

        replace->score=score;

        replace->flag=flag;

        replace->move=move;


    }



};



}



#endif