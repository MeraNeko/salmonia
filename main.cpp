// For lichess.org
#include "chess.hpp"
#include "search.hpp"
#include "polyglot_book.hpp"
#include "time_manager.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>

#include "nnue_wrapper.hpp"


/*
    单文件分发（Salmonia_lite）：
    NNUE 权重与 Polyglot 开局库均以 incbin
    嵌入可执行文件只读节，
    由 embedded_data.hpp 暴露访问入口
*/

#ifdef SALMONIA_LITE
#include "embedded_data.hpp"
#endif


// 线程池（Lazy SMP）：管理搜索工作线程与 NNUE 镜像
search::ThreadPool pool;


using namespace std;



int main()
{


    /*
        加载 NNUE 网络并分发到全部
        工作器（每工作器独立镜像）

        单文件版：数据在可执行文件内，无磁盘依赖
    */

#ifdef SALMONIA_LITE

    pool.load_nnue_embedded(
        "salmonia_30m.nnue [embedded]"
    );

#else

    pool.load_nnue(
        "salmonia_30m.nnue"
    );

#endif




    chess::Board board;


    // =========================================
    // Polyglot opening book
    // =========================================

    polyglot::Book book;


#ifdef SALMONIA_LITE

    if(book.load_from_memory(
           salmonia_embedded::book_data(),
           salmonia_embedded::book_size()))
    {
        cerr
        << "Polyglot book loaded (embedded), entries = "
        << book.size()
        << "\n";
    }
    else
    {
        cerr
        << "Polyglot book load failed\n";
    }

#else

    if(book.load("opening/gm2001.bin"))
    {
        cerr
        << "Polyglot book loaded, entries = "
        << book.size()
        << "\n";
    }
    else
    {
        cerr
        << "Polyglot book load failed\n";
    }

#endif



    string line;



    while(getline(cin,line))
    {

        stringstream ss(line);

        string cmd;

        ss >> cmd;



        /*
            UCI初始化
        */

        if(cmd=="uci")
        {

            cout
            << "id name "
#ifdef SALMONIA_LITE
            << "Salmonia v5.3.0 lite\n"
#else
            << "Salmonia v5.3.0 alpha\n"
#endif
            << "id author MeraNeko\n"
            << "option name Threads type spin default 1 min 1 max 512\n"
            << "info string thread binding: "
            << pool.thread_binding_information_as_string()
            << "\n"
            << "uciok\n"
            << flush;

        }



        /*
            初始化
        */

        else if(cmd=="isready")
        {

            cout
            << "readyok\n"
            << flush;

        }



        /*
            选项设置：setoption name <id> value <v>

            目前支持 Threads（搜索线程数）：
            数量变化时销毁旧工作组释放资源，
            重建后重新加载网络
        */

        else if(cmd=="setoption")
        {

            string token;

            string name;

            string value;



            while(ss >> token)
            {

                if(token=="name")
                {

                    while(ss >> token
                          && token != "value")
                    {

                        if(!name.empty())
                            name += " ";


                        name += token;

                    }


                    /*
                        内层循环终止于 "value"，
                        此处直接读取其后的值
                    */

                    if(token=="value")
                        ss >> value;

                }


                else if(token=="value")
                {

                    ss >> value;

                }

            }



            if(name=="Threads")
            {

                int n = 1;


                try
                {
                    n = stoi(value);
                }
                catch(...)
                {
                    n = 1;
                }


                pool.set_threads(n);

            }

        }



        /*
            新游戏
        */

        else if(cmd=="ucinewgame")
        {

            board = chess::Board();


            /*
                清理旧线程资源：清空转表
                与全部工作器的搜索状态
                （history/killer/PV/计数）
            */

            pool.clear();

        }




        /*
            设置局面

            position startpos

            position fen xxx moves ...
        */

        else if(cmd=="position")
        {

            string type;

            ss >> type;



            if(type=="startpos")
            {

                board = chess::Board();

            }

            else if(type=="fen")
            {

                /*
                    逐 token 读取 FEN 字段，
                    遇到 "moves" 停止。

                    注意：不能用 getline，
                    否则会把 moves 列表一起吞掉，
                    导致已走着法丢失
                */

                string fen;

                string tok;



                while(ss >> tok)
                {

                    if(tok=="moves")
                        break;



                    if(!fen.empty())
                        fen += " ";


                    fen += tok;

                }



                board =
                    chess::Board(fen);

            }




            /*
                执行moves
            */


            string word;



            while(ss >> word)
            {

                if(word=="moves")
                    continue;



                chess::Move move =
                    chess::uci::uciToMove(
                        board,
                        word
                    );



                if(move != chess::Move::NO_MOVE)
                {
                    board.makeMove(move);
                }

            }

        }





        /*
            思考
        */

        else if(cmd=="go")
        {


            int depth = 10;


            bool depthLimited = false;
            bool timeLimited = false;


            int wtime = 0;
            int btime = 0;
            int movestogo = 0;



            string token;



            while(ss >> token)
            {


                if(token=="depth")
                {

                    ss >> depth;

                    depthLimited = true;

                }


                else if(token=="wtime")
                {

                    ss >> wtime;

                    timeLimited = true;

                }


                else if(token=="btime")
                {

                    ss >> btime;

                    timeLimited = true;

                }


                else if(token=="movestogo")
                {

                    ss >> movestogo;

                }

            }





            /*
                时间模式：

                go wtime xxx btime xxx

                不限制深度，
                使用最大深度 + timer退出
            */

            if(timeLimited && !depthLimited)
            {
                depth = 64;
            }






            chess::Move bestMove;



            int score = 0;





            auto start =
                chrono::steady_clock::now();






            // =========================================
            // Try Polyglot book first
            // =========================================

            uint64_t key =
                polyglot::hash(board);


            polyglot::BookMove bookMove =
                book.choose(key);


            bool fromBook = false;


            if(!bookMove.uci.empty())
            {

                chess::Move bm =
                    chess::uci::uciToMove(
                        board,
                        bookMove.uci
                    );


                /*
                    库着法在当前局面非法时忽略，
                    回落到正常搜索
                */

                if(bm != chess::Move::NO_MOVE)
                {

                    bestMove = bm;

                    fromBook = true;


                    cout
                    << "info string book "
                    << bookMove.uci
                    << "\n";

                }

            }



            if(!fromBook)
            {


                /*
                    Time control
                */


                if(timeLimited)
                {


                    int remain;



                    if(board.sideToMove()
                        ==
                        chess::Color::WHITE)
                    {
                        remain = wtime;
                    }
                    else
                    {
                        remain = btime;
                    }




                    if(remain > 0)
                    {

                        int limit =
                            engine::TimeManager::calculateTime(
                                remain,
                                movestogo
                            );


                        engine::timer.startTimer(limit);

                    }

                }


                else
                {

                    /*
                        纯深度模式：重置计时器为极大时限，
                        防止上一次 go wtime 残留的
                        短时限提前中断本次搜索
                    */

                    engine::timer.startTimer(
                        2000000000
                    );

                }






                score =
                    pool.searchBestMove(
                        board,
                        depth,
                        bestMove
                    );


            }







            auto end =
                chrono::steady_clock::now();





            auto ms =
                chrono::duration_cast
                <
                    chrono::milliseconds
                >
                (end-start)
                .count();







            /*
                聚合全部线程的节点数，
                计算 NPS 一并报告
            */

            uint64_t totalNodes =
                pool.nodes_searched();


            uint64_t nps =
                ms > 0
                    ? totalNodes * 1000 / ms
                    : 0;



            /*
                开局库着法没有搜索统计可报，
                跳过（避免输出上一轮的陈旧数据）
            */

            if(!fromBook)
            {

            cout
            << "info depth "
            << search::completedDepth
            << " score cp "
            << score
            << " nodes "
            << totalNodes
            << " nps "
            << nps
            << " time "
            << ms
            << "\n";

            }







            /*
                无着法局面（将杀/逼和）：
                输出 0000 而非默认构造的伪着法
            */

            if(bestMove == chess::Move{})
            {
                cout << "bestmove 0000\n" << flush;
            }

            else
            {

            cout
            << "bestmove "
            << chess::uci::moveToUci(
                    bestMove
            )
            << "\n"
            << flush;

            }



        }






        /*
            停止搜索
        */

        else if(cmd=="stop")
        {

            // TODO

        }





        /*
            退出
        */

        else if(cmd=="quit")
        {

            break;

        }

    }



    return 0;

}