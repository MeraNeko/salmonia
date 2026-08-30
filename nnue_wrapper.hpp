#pragma once


#include <deque>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "chess.hpp"


#include "attacks.h"
#include "evaluate.h"
#include "position.h"
#include "types.h"

#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include "nnue/nnue_misc.h"



namespace salmonia_nnue
{


/*
    Salmonia <-> Stockfish NNUE 适配层

    完整复用 Stockfish 的 NNUE 推理链路：

        Network (加载 .nnue 权重)
            |
        FeatureTransformer (特征变换 / 累加器)
            |
        NetworkArchitecture (前向传播)

    不裁剪任何 Feature / Layer，
    不修改 NNUE 内部结构。
*/


class NNUE
{

public:

    NNUE()
    {

        init_stockfish_once();

    }



    NNUE(const NNUE&) = delete;
    NNUE& operator=(const NNUE&) = delete;



    /*
        加载 .nnue 网络文件

        若指定文件加载失败（例如架构哈希不匹配），
        自动回退到 Stockfish 默认网络
        (EvalFileDefaultName)
    */

    bool load(const std::string& filename)
    {

        if(load_file(filename))
            return true;


        std::cerr
            << "NNUE fallback: trying "
            << EvalFileDefaultName
            << "\n";


        if(load_file(
            EvalFileDefaultName
        ))
            return true;


        /*
            默认网络在 stockfish/src 目录下，
            工作目录查找失败时补充路径重试；
            仍失败则保持未加载状态，
            由 evaluateSTM 回退到手写评估
        */

        std::string nested =
            std::string("stockfish/src/")
            + EvalFileDefaultName;


        std::cerr
            << "NNUE fallback: trying "
            << nested
            << "\n";


        return load_file(nested);

    }



    /*
        加载内嵌于可执行文件的网络

        走 Stockfish 自身的内存流加载路径
        Network::load_internal()：
        读取链接进只读节的 gEmbeddedNNUEData
        （由 embedded_data.cpp 通过 incbin 提供，
        本目标以 -DUNIVERSAL_BINARY 编译，
        network.cpp 内该符号退化为 extern 引用）。

        无磁盘访问、无文件回退；
        label 仅用于日志标识。
    */

    bool load_embedded(const std::string& label)
    {

        try
        {

            if(!network)
            {

                network =
                    std::make_unique
                    <
                        Stockfish::Eval::NNUE::Network
                    >();

            }


            evalFile.current.reset();


            network->load_internal(
                evalFile
            );


            return finish_load(
                evalFile.current.has_value(),
                label);

        }
        catch(const std::exception& e)
        {
            std::cerr
                << "NNUE exception: "
                << e.what()
                << "\n";

            return false;
        }

    }



    /*
        对已构造好的 Stockfish Position
        进行静态评估

        返回值：以走子方视角计的评估值
    */

    int evaluate(Stockfish::Position& pos)
    {

        if(!loaded)
            return 0;


        /*
            每次评估使用全新的累加器栈，
            强制走全量刷新路径，
            不依赖走棋历史
        */

        auto accumulators =
            std::make_unique
            <
                Stockfish::Eval::NNUE::AccumulatorStack
            >();

        accumulators->reset();


        return int(
            Stockfish::Eval::evaluate(
                *network,
                pos,
                *accumulators,
                *caches,
                Stockfish::VALUE_ZERO
            )
        );

    }



    /*
        直接由 FEN 字符串评估

        返回值：以走子方视角计的评估值

        FEN 非法时返回 0
    */

    int evaluate_fen(const std::string& fen)
    {

        if(!loaded)
            return 0;


        Stockfish::StateInfo si;

        Stockfish::Position pos;


        auto err =
            pos.set(
                fen,
                false,
                &si
            );


        if(err.has_value())
            return 0;


        return evaluate(pos);

    }



    /*
        增量评估接口

        依赖与外部 chess::Board 保持同步的
        长存活镜像 Position 与累加器栈：

            sync_reset  -> 根局面重建镜像（每次搜索一次）
            sync_move   -> 镜像 board.makeMove
            sync_unmove -> 镜像 board.unmakeMove
            sync_null   -> 镜像 board.makeNullMove
            sync_unnull -> 镜像 board.unmakeNullMove

        调用 NNUE 时走增量路径：
        AccumulatorStack 复用最接近的已算累加器，
        仅按脏棋子做前向增量更新；无可用累加器时
        由 Finny 缓存做廉价刷新而非全量重建。

        若检测到镜像与 Board 状态脱钩
        （未经同步钩子的走棋），自动按需重建，
        保证结果与全量重建完全一致。

        返回值：以走子方视角计的评估值
    */

    int evaluate(chess::Board& board)
    {

        if(!loaded)
            return 0;


        if(!matches(board))
        {

            sync_reset(board);


            if(!synced)
                return 0;

        }


        return int(
            Stockfish::Eval::evaluate(
                *network,
                *pos,
                *accStack,
                *caches,
                Stockfish::VALUE_ZERO
            )
        );

    }



    /*
        由根局面重建镜像 Position 并重置累加器栈。
        每次搜索（searchBestMove）开始时调用一次。

        注意：本层不无条件信任传入的 Board ——
        pos->set 成功后立即回验棋子分布，
        防止 set 解析与 Board 实际状态不一致时
        （如 FEN 边界情况）进入假同步状态
    */

    void sync_reset(const chess::Board& board)
    {

        synced = false;


        if(!loaded)
            return;


        ensure_runtime();


        rootState =
            std::make_unique
            <
                Stockfish::StateInfo
            >();


        states.clear();

        accStack->reset();


        auto err =
            pos->set(
                board.getFen(),
                false,
                rootState.get()
            );


        if(err.has_value())
            return;


        synced = true;


        if(!matches(board))
            synced = false;

    }



    /*
        搜索钩子：与 chess::Board::makeMove 配对调用。
        Position::do_move 同时填充累加器脏信息，
        供后续增量更新使用。
    */

    void sync_move(chess::Move move)
    {

        if(!synced)
            return;


        /*
            前置安全阀：若镜像出发格上不是棋子，
            说明镜像已与实际局面脱钩，
            放弃同步由 evaluate 的按需重建兜底
        */

        if(pos->piece_on(
                Stockfish::Square(
                    move.from().index()))
            == Stockfish::NO_PIECE)
        {

            synced = false;

            return;

        }


        // 深度安全阀：超限后放弃同步，
        // 由 evaluate 的按需重建兜底

        if(states.size() >= SYNC_MAX_PLY)
        {
            synced = false;
            return;
        }


        Stockfish::Move m =
            sf_move(move);


        states.emplace_back();


        Stockfish::Dirties& dirties =
            accStack->push();


        pos->do_move(
            m,
            states.back(),
            pos->gives_check(m),
            dirties,
            nullptr,
            nullptr
        );

    }



    /*
        搜索钩子：与 chess::Board::unmakeMove 配对调用
    */

    void sync_unmove(chess::Move move)
    {

        if(!synced || states.empty())
            return;


        pos->undo_move(
            sf_move(move)
        );


        accStack->pop();

        states.pop_back();

    }



    /*
        搜索钩子：镜像空着法。
        空着法不改变棋子分布，
        无需压入累加器栈（与 Stockfish 一致）
    */

    void sync_null()
    {

        if(!synced)
            return;


        if(states.size() >= SYNC_MAX_PLY)
        {
            synced = false;
            return;
        }


        states.emplace_back();


        pos->do_null_move(
            states.back()
        );

    }



    void sync_unnull()
    {

        if(!synced || states.empty())
            return;


        pos->undo_null_move();

        states.pop_back();

    }



    bool is_loaded() const
    {

        return loaded;

    }



    // 实际加载成功的网络文件名

    const std::string& loaded_file() const
    {

        return loadedFile;

    }



private:

    /*
        实际执行单个网络文件的加载
    */

    bool load_file(const std::string& filename)
    {

        try
        {

            if(!network)
            {
                network =
                    std::make_unique
                    <
                        Stockfish::Eval::NNUE::Network
                    >();
            }


            evalFile.current.reset();


            network->load(
                std::filesystem::path{},
                filename,
                evalFile
            );


            return finish_load(
                evalFile.current.has_value(),
                filename);

        }
        catch(const std::exception& e)
        {
            std::cerr
                << "NNUE exception: "
                << e.what()
                << "\n";

            return false;
        }

    }



    /*
        加载收尾：登记状态、按已加载网络
        构造累加器缓存并报告

        文件加载与内嵌加载共用，
        日志格式与既往完全一致
    */

    bool finish_load(bool ok, const std::string& label)
    {

        loaded = ok;


        if(loaded)
        {

            loadedFile = label;


            /*
                累加器缓存 (Finny tables)
                必须基于已加载的网络构造
            */

            caches =
                std::make_unique
                <
                    Stockfish::Eval::NNUE::AccumulatorCaches
                >(
                    *network
                );


            std::cout
                << "NNUE loaded: "
                << label
                << "\n";

        }

        else
        {

            std::cerr
                << "NNUE load failed: "
                << label
                << "\n";

        }


        return loaded;

    }



    /*
        Stockfish 运行时环境初始化

        Attacks::init()  -> 攻击表 (full_threats 特征依赖)
        Position::init() -> Zobrist 键表

        整个进程只需执行一次
    */

    static void init_stockfish_once()
    {

        static bool done = false;


        if(!done)
        {

            Stockfish::Attacks::init();

            Stockfish::Position::init();


            done = true;

        }

    }



    /*
        着法编码转换：
        chess-library 与 Stockfish 的 16 位着法编码
        逐位一致（from<<6|to、promo<<12、type<<14），
        升变/过路兵/易位（王吃车编码）均无需变换
    */

    static Stockfish::Move sf_move(chess::Move move)
    {

        return Stockfish::Move(
            std::uint16_t(move.move())
        );

    }



    /*
        棋子编码转换：
        chess::Piece  WHITEPAWN..WHITEKING = 0..5，
                      BLACKPAWN..BLACKKING = 6..11，
                      NONE = 12；
        Stockfish     W_PAWN..W_KING = 1..6，
                      B_PAWN..B_KING = 9..14
                      （make_piece = (color<<3)|type，
                        黑方 color 位为 8 而非 6），
                      NO_PIECE = 0
    */

    static Stockfish::Piece sf_piece(chess::Piece piece)
    {

        int v = static_cast<int>(piece);


        if(v == 12)
            return Stockfish::NO_PIECE;


        return Stockfish::Piece(
            (v / 6) * 8 + (v % 6) + 1
        );

    }



    /*
        镜像 Position 与外部 Board 的一致性校验：
        行棋方、全部 64 格棋子分布、半回合钟
        （后者参与 NNUE 的和棋分数衰减项）。
        任一不符即视为脱钩，由调用方重建。
    */

    bool matches(const chess::Board& board) const
    {

        if(!synced)
            return false;


        int stm =
            (board.sideToMove()
                == chess::Color::WHITE)
            ? Stockfish::WHITE
            : Stockfish::BLACK;


        if(int(pos->side_to_move()) != stm)
            return false;


        for(int sq = 0; sq < 64; ++sq)
        {

            if(pos->piece_on(
                    Stockfish::Square(sq))
                != sf_piece(
                    board.at(chess::Square(sq))))
                return false;

        }


        if(pos->rule50_count()
            != int(board.halfMoveClock()))
            return false;


        return true;

    }



    /*
        懒分配长存活运行时对象：
        镜像 Position 与累加器栈。
        累加器栈达数 MB，仅在需要时分配，
        未加载网络的测试程序不受影响。
    */

    void ensure_runtime()
    {

        if(!pos)
        {
            pos =
                std::make_unique
                <
                    Stockfish::Position
                >();
        }


        if(!accStack)
        {
            accStack =
                std::make_unique
                <
                    Stockfish::Eval::NNUE::AccumulatorStack
                >();
        }

    }



    // 镜像同步深度上限（低于 SF MAX_PLY=246），
    // 防止累加器栈/StateInfo 栈溢出

    static constexpr std::size_t SYNC_MAX_PLY = 220;



    std::unique_ptr<Stockfish::Eval::NNUE::Network>
        network;

    std::unique_ptr<Stockfish::Eval::NNUE::AccumulatorCaches>
        caches;

    Stockfish::Eval::NNUE::EvalFile evalFile;


    // 增量同步运行时状态

    std::unique_ptr<Stockfish::Position> pos;

    std::unique_ptr<Stockfish::StateInfo> rootState;

    // deque 保证元素地址稳定：
    // StateInfo::previous 链依赖指针不失效

    std::deque<Stockfish::StateInfo> states;

    std::unique_ptr<Stockfish::Eval::NNUE::AccumulatorStack>
        accStack;


    bool synced = false;


    bool loaded = false;


    std::string loadedFile;


};



}
