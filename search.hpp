#ifndef SEARCH_HPP
#define SEARCH_HPP


#include "chess.hpp"
#include "evaluate.hpp"
#include "transposition.hpp"
#include "time_manager.hpp"
#include "nnue_wrapper.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>


namespace search
{


inline std::atomic<bool> stopSearch{false};
inline uint64_t nodes=0;
inline int completedDepth = 0;

constexpr int INF = 1000000;
constexpr int MATE_SCORE = 60000;
constexpr int MAX_PLY = 128;
constexpr int MAX_EXTENSION = 2;



// Null Move 剪枝参数（可用编译宏覆盖，供 tuner 网格搜索）

#ifndef SALMONIA_NMP_MIN_DEPTH
#define SALMONIA_NMP_MIN_DEPTH 3
#endif

#ifndef SALMONIA_NMP_MAX_R
#define SALMONIA_NMP_MAX_R 4
#endif

constexpr int NMP_MIN_DEPTH = SALMONIA_NMP_MIN_DEPTH;   // 启用 NMP 的最小深度
constexpr int NMP_MAX_R     = SALMONIA_NMP_MAX_R;       // 缩减量上限



// LMR 参数（可用编译宏覆盖，供 tuner 网格搜索）

#ifndef SALMONIA_LMR_MIN_DEPTH
#define SALMONIA_LMR_MIN_DEPTH 3
#endif

#ifndef SALMONIA_LMR_FULL_MOVES
#define SALMONIA_LMR_FULL_MOVES 3
#endif

#ifndef SALMONIA_LMR_LOW_HISTORY
#define SALMONIA_LMR_LOW_HISTORY 20000
#endif

constexpr int LMR_MIN_DEPTH = SALMONIA_LMR_MIN_DEPTH;      // 父节点最小深度
constexpr int LMR_FULL_MOVES = SALMONIA_LMR_FULL_MOVES;    // 前 N 个走法全深度搜索
constexpr int LMR_LOW_HISTORY = SALMONIA_LMR_LOW_HISTORY;  // 低历史分阈值



/*
    残局保护：行棋方除国王/兵外无进攻子力时
    禁用 Null Move（zugzwang 高发）
*/

inline bool hasNonPawnMaterial(const chess::Board& board)
{

    auto side =
        board.sideToMove();


    auto minors =
        board.pieces(
            chess::PieceType::KNIGHT,
            chess::PieceType::BISHOP,
            chess::PieceType::ROOK,
            chess::PieceType::QUEEN
        );


    return (minors & board.us(side)).count() > 0;

}



inline TranspositionTable TT(64);



/*
    将杀分数的 TT 存取校正：

    搜索中的将杀分数是"距离根节点的 ply"相对的
    （-MATE_SCORE + ply），直接存入 TT 会在
    不同 ply 复用时产生错误的将杀距离。
    存取时统一换算为"距当前局面的将杀步数"。
*/

inline int scoreToTT(int score, int ply)
{

    if(score > MATE_SCORE)
        return score - ply;


    if(score < -MATE_SCORE)
        return score + ply;


    return score;

}



inline int scoreFromTT(int score, int ply)
{

    if(score > MATE_SCORE)
        return score + ply;


    if(score < -MATE_SCORE)
        return score - ply;


    return score;

}



/*
    搜索工作线程状态（Lazy SMP）：

    每个线程持有独立的 history/killer/PV 表、
    节点计数与 NNUE 镜像（镜像含长存活
    Position 与累加器栈，不可共享）；
    转表 TT 全局共享（与 Stockfish 一致，
    良性竞争可接受）。

    TT move 完全信任、不做合法性校验：
    它只参与走法排序，非法着法不在
    生成列表中、永远不会被实际搜索。
*/

struct SearchWorker
{

    int idx = 0;


    salmonia_nnue::NNUE nnue;


    chess::Move killerMoves[MAX_PLY][2];


    chess::Move pvTable[MAX_PLY];


    int historyTable[64][64];


    uint64_t nodes = 0;

    int completedDepth = 0;

    chess::Move bestMove{};

    int score = 0;



    void resetSearchState()
    {

        memset(
            historyTable,
            0,
            sizeof(historyTable)
        );


        memset(
            killerMoves,
            0,
            sizeof(killerMoves)
        );



        memset(
            pvTable,
            0,
            sizeof(pvTable)
        );



        nodes = 0;

        completedDepth = 0;

        bestMove = chess::Move{};

        score = 0;

    }

};




// ============================================================
// Evaluation
// ============================================================


inline int evaluateSTM(
    SearchWorker& w,
    chess::Board& board
)
{

    /*
        优先使用 NNUE：
        wrapper 返回值已是行棋方视角（STM），
        无需再取反。

        将杀区间保护：NNUE 内部将杀分数
        与搜索引擎的 MATE_SCORE 体系不同，
        夹入非将杀区间避免干扰将杀判定。
    */

    if(w.nnue.is_loaded())
    {

        int score =
            w.nnue.evaluate(board);


        return
            std::max(
                -(MATE_SCORE - 1),
                std::min(
                    MATE_SCORE - 1,
                    score
                )
            );

    }



    /*
        回退：手写评估为白方视角，
        黑方行棋时取反转为 STM 视角
    */

    int score =
        evaluate(board);



    if(board.sideToMove()
        == chess::Color::WHITE)
        return score;


    return -score;

}




// ============================================================
// Move ordering
// ============================================================


inline int moveScore(
    SearchWorker& w,
    const chess::Board& board,
    chess::Move move,
    int ply,
    chess::Move ttMove
)
{

    int score = 0;



    // TT move

    if(move == ttMove)
        score += 10000000;



    // PV move

    if(ply < MAX_PLY
       &&
       move == w.pvTable[ply])
    {
        score += 9000000;
    }





    // captures

    if(board.isCapture(move))
    {

        auto victim =
            board.at(move.to());


        auto attacker =
            board.at(move.from());



        score +=
            100000
            +
            10 *
            pieceValue[
                static_cast<int>(
                    victim.type()
                )
            ]
            -
            pieceValue[
                static_cast<int>(
                    attacker.type()
                )
            ];

    }

    else
    {


        if(ply < MAX_PLY)
        {

            if(move == w.killerMoves[ply][0])
                score += 90000;


            else if(move == w.killerMoves[ply][1])
                score += 80000;

        }




        score +=
            w.historyTable
            [
                move.from().index()
            ]
            [
                move.to().index()
            ];

    }



    return score;

}






inline void orderMoves(
    SearchWorker& w,
    const chess::Board& board,
    chess::Movelist& moves,
    chess::Move ttMove,
    int ply
)
{

    std::sort(
        moves.begin(),
        moves.end(),

        [&](const auto& a,const auto& b)
        {

            return
            moveScore(
                w,
                board,
                a,
                ply,
                ttMove
            )
            >
            moveScore(
                w,
                board,
                b,
                ply,
                ttMove
            );

        }

    );

}






// ============================================================
// Quiescence
// ============================================================


int quiescence(
    SearchWorker& w,
    chess::Board& board,
    int alpha,
    int beta,
    int ply
)
{
    w.nodes++;

    if((w.nodes & 1023)==0)
    {
        if(engine::timer.isTimeUp())
        {
            stopSearch=true;
            return 0;
        }
    }


    int best =
        evaluateSTM(w, board);



    /*
        失败软（fail-soft）：始终返回真实最佳值
        而非窗口边界。PVS 的零窗口侦察依赖
        失败高/低时带回的真实分数判断是否重搜；
        若返回窗口边界（fail-hard），会丢失
        信息导致边界判断失真。
    */

    if(best >= beta)
        return best;



    if(best > alpha)
        alpha = best;





    chess::Movelist moves;


    chess::movegen::legalmoves(
        moves,
        board
    );



    orderMoves(
        w,
        board,
        moves,
        chess::Move{},
        ply
    );





    for(auto move:moves)
    {


        // 目前只搜索吃子

        if(!board.isCapture(move))
            continue;



        w.nnue.sync_move(move);

        board.makeMove(move);



        int score =
            -quiescence(
                w,
                board,
                -beta,
                -alpha,
                ply+1
            );



        board.unmakeMove(move);

        w.nnue.sync_unmove(move);




        if(score > best)
            best = score;



        if(best >= beta)
            return best;



        if(best > alpha)
            alpha = best;

        if (stopSearch) break;

    }




    return best;

}

inline int extension(
    const chess::Board& board,
    chess::Move move,
    int extSum
)
{
    // 沿路径累计延伸量限制，
    // 防止连续将军导致无限延伸

    if(extSum >= MAX_EXTENSION)
        return 0;


    int ext=0;


    // check
    if(board.inCheck())
        ext++;


    /*
        升变延伸：makeMove 后 from 格已空，
        必须用着法类型判断而非盘面查询
    */

    if(move.typeOf()
        == chess::Move::PROMOTION)
        ext++;


    return std::min(ext,1);
}

// ============================================================
// Alpha Beta Negamax
// ============================================================


int alphaBeta(
    SearchWorker& w,
    chess::Board& board,
    int depth,
    int alpha,
    int beta,
    int ply,
    int extSum = 0,
    bool fromLMR = false
)
{
    w.nodes++;


    if((w.nodes & 2047)==0)
    {
        if(engine::timer.isTimeUp())
        {
            stopSearch=true;
            return 0;
        }
    }


    bool inCheck =
        board.inCheck();



    if(depth <= 0)
    {

        // 被将军的叶子节点延伸一层，
        // 避免把将杀/逼和误判为静局

        if(inCheck && extSum < MAX_EXTENSION)
            depth = 1;

        else
            return quiescence(
                w,
                board,
                alpha,
                beta,
                ply
            );

    }





    int originalAlpha = alpha;



    chess::Move ttMove{};


    int ttScore;



    if(TT.probe(
        board,
        depth,
        alpha,
        beta,
        ttScore,
        ttMove
    ))
    {

        /*
            TT move 的合法性已在 probe 内校验，
            此处仅做将杀分数 ply 校正后返回。

            边界类型语义由 probe 保证：
            EXACT      -> 精确值
            LOWERBOUND -> 仅在 >= beta 时截断
            UPPERBOUND -> 仅在 <= alpha 时截断
        */

        return scoreFromTT(ttScore, ply);
    }



    // ============================================================
    // Null Move 剪枝：
    //
    // 跳过己方回合，若对手即使多走一步
    // 仍无法突破 beta，则当前局面必然
    // 足够好，直接剪枝。
    //
    // 保护条件：
    //   - 不被将军
    //   - 非根节点且深度足够
    //   - 非纯兵残局（zugzwang）
    //   - 将杀分数回夹到 beta（不虚构将杀）
    // ============================================================

    if(!inCheck
       &&
       ply > 0
       &&
       depth >= NMP_MIN_DEPTH
       &&
       hasNonPawnMaterial(board))
    {

        // 动态缩减量 R = 2 + depth/4，封顶 NMP_MAX_R

        int R =
            std::min(
                2 + depth / 4,
                NMP_MAX_R
            );



        w.nnue.sync_null();

        board.makeNullMove();


        int nullScore =
            -alphaBeta(
                w,
                board,
                depth - 1 - R,
                -beta,
                -beta + 1,
                ply + 1,
                0
            );


        board.unmakeNullMove();

        w.nnue.sync_unnull();



        if(stopSearch)
            return 0;



        if(nullScore >= beta)
        {

            /*
                不返回真实将杀分数，
                避免基于空着法的虚假将杀声明；
                非将杀时失败软返回真实分数。

                注意：NMP 截断本身不写入 TT
                （直接 return，跳过末尾 store）；
                空着法证明的下界不可靠，
                会污染后续零窗口侦察的边界判断。
            */

            if(nullScore > MATE_SCORE)
                return beta;


            return nullScore;

        }

    }







    chess::Movelist moves;



    chess::movegen::legalmoves(
        moves,
        board
    );



    if(moves.empty())
    {

        if(inCheck)
            return
                -MATE_SCORE + ply;


        return 0;

    }







    orderMoves(
        w,
        board,
        moves,
        ttMove,
        ply
    );






    int bestScore =
        -INF;



    chess::Move bestMove{};



    int moveCount = 0;





    for(auto move:moves)
    {

        moveCount++;



        /*
            着法属性必须在 makeMove 之前缓存：
            chess.hpp 的 isCapture 依赖目标格占用，
            makeMove 之后目标格已被己方棋子占据，
            会把所有着法误判为吃子
        */

        bool isCap =
            board.isCapture(move);


        bool isPromo =
            move.typeOf()
            == chess::Move::PROMOTION;



        w.nnue.sync_move(move);

        board.makeMove(move);


        int ext =
            extension(
                board,
                move,
                extSum
            );



        // ============================================================
        // LMR：对排序靠后的安静走法缩减深度。
        //
        // 适用条件：非吃子、非升变、非将军、
        // 深度与序号超过阈值。
        // 缩减量由走法序号、深度、历史分
        // 共同决定；若缩减搜索意外失败
        // （score > alpha），则全深度重搜。
        // ============================================================

        int newDepth =
            depth - 1 + ext;


        bool doReduce =
            depth >= LMR_MIN_DEPTH
            &&
            moveCount > LMR_FULL_MOVES
            &&
            !isCap
            &&
            !isPromo
            &&
            !inCheck;


        int r = 0;


        if(doReduce)
        {

            // 基础缩减量随序号与深度增长

            r =
                1
                +
                (moveCount > 8)
                +
                (depth > 6);


            // 历史分低的走法额外缩减，
            // 历史分高的走法不缩减

            int hist =
                w.historyTable
                [
                    move.from().index()
                ]
                [
                    move.to().index()
                ];


            if(hist < LMR_LOW_HISTORY)
                r++;


            else if(hist > 60000)
                r--;



            r =
                std::max(0, r);


            r =
                std::min(r, newDepth);


            newDepth -= r;

        }



        /*
            PVS：第一个着法全窗口搜索；
            其余着法先零窗口（-alpha-1, -alpha），
            失败高再全窗口重搜。
        */

        int score;


        if(moveCount == 1)
        {

            score =
                -alphaBeta(
                    w,
                    board,
                    newDepth,
                    -beta,
                    -alpha,
                    ply+1,
                    extSum+ext,
                    false
                );

        }

        else
        {

            score =
                -alphaBeta(
                    w,
                    board,
                    newDepth,
                    -alpha - 1,
                    -alpha,
                    ply+1,
                    extSum+ext,
                    r > 0
                );



            // 缩减搜索意外失败：全深度零窗口重搜

            if(score > alpha
               &&
               r > 0)
            {

                score =
                    -alphaBeta(
                        w,
                        board,
                        depth - 1 + ext,
                        -alpha - 1,
                        -alpha,
                        ply+1,
                        extSum+ext,
                        false
                    );

            }



            // 零窗口失败高且窗口允许：全窗口重搜

            if(score > alpha
               &&
               score < beta)
            {

                score =
                    -alphaBeta(
                        w,
                        board,
                        depth - 1 + ext,
                        -beta,
                        -alpha,
                        ply+1,
                        extSum+ext,
                        false
                    );

            }

        }


        board.unmakeMove(move);

        w.nnue.sync_unmove(move);



        if(stopSearch)
            break;





        if(score > bestScore)
        {

            bestScore = score;

            bestMove = move;

        }






        if(score > alpha)
        {

            alpha = score;



            // PV

            if(ply < MAX_PLY)
            {
                w.pvTable[ply]=move;
            }







            // History

            if(!isCap
               &&
               ply < MAX_PLY)
            {

                int& h =
                    w.historyTable
                    [
                        move.from().index()
                    ]
                    [
                        move.to().index()
                    ];



                h += depth * depth;



                if(h > 100000)
                    h = 100000;

            }


        }







        if(alpha >= beta)
        {



            // Killer

            if(!isCap
               &&
               ply < MAX_PLY)
            {


                if(w.killerMoves[ply][0]
                    != move)
                {

                    w.killerMoves[ply][1]
                        =
                    w.killerMoves[ply][0];


                    w.killerMoves[ply][0]
                        =
                    move;

                }

            }



            break;

        }
        if (stopSearch) break;


    }








    TTFlag flag;



    if(bestScore <= originalAlpha)
    {
        flag =
            TTFlag::UPPERBOUND;
    }

    else if(bestScore >= beta)
    {
        flag =
            TTFlag::LOWERBOUND;
    }

    else
    {
        flag =
            TTFlag::EXACT;
    }






    // ============================================================
    // TT 存储：
    //
    // LMR 缩减搜索的浅深度结果不写入 TT，
    // 避免污染后续全深度搜索的命中项
    // ============================================================

    if(!fromLMR)
    {

    TT.store(
        board,
        depth,
        scoreToTT(bestScore, ply),
        flag,
        bestMove,
        ply
    );

    }



    return bestScore;

}





// ============================================================
// Root PVS Search
//
// 根节点显式搜索全部合法着法：
// 第一个着法全窗口，其余 PVS 零窗口 + 失败高重搜；
// bestMove 由根循环直接维护，不依赖 TT 提取；
// 完成后将根结果写入 TT 供下一层排序使用。
// ============================================================



















int rootSearch(
    SearchWorker& w,
    chess::Board& board,
    int depth,
    chess::Move& bestMove,
    int alpha = -INF,
    int beta = INF
)
{


    chess::Movelist moves;



    chess::movegen::legalmoves(
        moves,
        board
    );





    chess::Move ttMove =
        TT.getMove(board);




    orderMoves(
        w,
        board,
        moves,
        ttMove,
        0
    );




    /*
        记录入窗下界：根搜索结束后按
        分数与窗口的关系决定 TT 边界类型
    */

    int originalAlpha = alpha;




    chess::Move currentBest{};


    int moveCount = 0;






    for(auto move:moves)
    {
        if(stopSearch)
            break;



        moveCount++;



        w.nnue.sync_move(move);

        board.makeMove(move);




        int ext =
            extension(
                board,
                move,
                0
            );



        int newDepth =
            depth - 1 + ext;



        int score;



        if(moveCount == 1)
        {

            // 第一个着法：全窗口搜索

            score =
                -alphaBeta(
                    w,
                    board,
                    newDepth,
                    -beta,
                    -alpha,
                    1,
                    ext
                );

        }

        else
        {

            // PVS：先零窗口侦察

            score =
                -alphaBeta(
                    w,
                    board,
                    newDepth,
                    -alpha - 1,
                    -alpha,
                    1,
                    ext
                );



            // 失败高：全窗口重搜取精确分数

            if(score > alpha
               &&
               score < beta)
            {

                score =
                    -alphaBeta(
                        w,
                        board,
                        newDepth,
                        -beta,
                        -alpha,
                        1,
                        ext
                    );

            }

        }



        board.unmakeMove(move);

        w.nnue.sync_unmove(move);



        if(stopSearch)
            break;







        if(score > alpha)
        {

            alpha = score;


            currentBest =
                move;


            w.pvTable[0] = move;

        }

    }





    bestMove =
        currentBest;



    /*
        根节点写入 TT：
        完整搜完时为 EXACT，供下一层深度的
        根着法排序使用；被时间中断则不写，
        避免污染上一层的可靠条目。

        窄窗口搜索按边界类型写入：
        fail-low  -> UPPERBOUND
        fail-high -> LOWERBOUND
        收敛      -> EXACT
    */

    if(!stopSearch
       &&
       currentBest != chess::Move{})
    {

        TTFlag flag;


        if(alpha <= originalAlpha)
        {
            flag = TTFlag::UPPERBOUND;
        }

        else if(alpha >= beta)
        {
            flag = TTFlag::LOWERBOUND;
        }

        else
        {
            flag = TTFlag::EXACT;
        }


        TT.store(
            board,
            depth,
            alpha,
            flag,
            currentBest,
            0
        );

    }



    return alpha;

}







// ============================================================
// Main Search（单线程工作器根搜索）
// ============================================================


int workerSearchRoot(
    SearchWorker& w,
    chess::Board& board,
    int maxDepth
)
{

    w.resetSearchState();





    chess::Move bestMove =
        chess::Move{};



    // 无着法局面（将杀/逼和）直接返回

    chess::Movelist rootMoves;

    chess::movegen::legalmoves(
        rootMoves,
        board
    );


    if(rootMoves.empty())
    {
        w.score =
            board.inCheck() ? -MATE_SCORE : 0;

        return w.score;
    }



    // 兜底着法：时间耗尽时至少有一个合法着法

    bestMove =
        rootMoves[0];



    /*
        建立 NNUE 镜像同步：每次搜索仅在此处
        做一次 FEN 重建，之后搜索全程走
        do_move/undo_move 增量路径
    */

    w.nnue.sync_reset(board);



    int score = 0;







    for(int depth=1;
    depth<=maxDepth;
    depth++)
    {

        /*
            辅助线程深度调度（Lazy SMP）：
            偶数号辅助线程在部分层加深一层，
            与主线程错峰，增加探索多样性
        */

        int d = depth;


        if(w.idx > 0
           &&
           (w.idx % 2 == 0)
           &&
           depth < maxDepth)
            d = depth + 1;



        /*
            每层深度由根节点 PVS 搜索完成。

            Aspiration Window（渐进窗口）：
            第 5 层起以上一层分数为中心取窄窗口，
            失败后按指数加宽并重搜（窗口外的重搜
            天然是全深度）。窄窗口使大量子树以更低
            深度命中 TT 截断，显著减少节点数。

            浅层不启用：深度 1~4 全窗口搜索本身
            很便宜，而浅层分数波动大，过早启用会
            在尖锐局面引发反复重搜，得不偿失。

            分数提交规则（对外报告可靠性）：
            只有窗口收敛或最后一次全窗口重搜完成
            时才提交 score / bestMove / completedDepth；
            时间中断发生在窗口循环内则保留上一层
            已完成的结果，绝不报告半成品分数。

            将杀区间（|score| 接近 MATE_SCORE）
            跳过窄窗口，避免将杀分数跳变引发
            反复重搜。
        */

        chess::Move depthBest{};


        int s;


        bool useAspiration =
            depth >= 5
            &&
            std::abs(score) < MATE_SCORE - 256;


        if(useAspiration)
        {

            int delta = 50;

            int alpha =
                std::max(score - delta, -INF);

            int beta =
                std::min(score + delta, INF);


            for(;;)
            {

                s =
                    rootSearch(
                        w,
                        board,
                        d,
                        depthBest,
                        alpha,
                        beta
                    );


                if(stopSearch)
                    break;


                if(s <= alpha)
                {

                    // fail-low：压低下界并收窄上界，
                    // 加宽步长后重搜

                    beta = (alpha + beta) / 2;

                    alpha =
                        std::max(s - delta, -INF);

                }

                else if(s >= beta)
                {

                    // fail-high：抬升上界后重搜

                    beta =
                        std::min(s + delta, INF);

                }

                else
                {

                    // 窗口收敛：分数精确

                    break;

                }


                delta *= 2;


                // 窗口已足够宽：直接全窗口重搜定案

                if(delta > 1000)
                {

                    s =
                        rootSearch(
                            w,
                            board,
                            d,
                            depthBest,
                            -INF,
                            INF
                        );

                    break;

                }

            }

        }

        else
        {

            s =
                rootSearch(
                    w,
                    board,
                    d,
                    depthBest
                );

        }


        if(stopSearch)
            break;



        score = s;


        if(depthBest != chess::Move{})
            bestMove = depthBest;


        w.completedDepth = d;

    }





    w.bestMove = bestMove;

    w.score = score;


    return score;

}




// ============================================================
// ThreadPool（Lazy SMP）
//
// 主线程（worker 0）负责协调并输出结果，
// 辅助线程在各自的 Board 副本上并行搜索；
// 转表共享，history/killer/PV 与 NNUE 镜像私有。
// ============================================================


class ThreadPool
{

public:

    ThreadPool()
    {
        ensure_workers(1);
    }



    size_t num_threads() const
    {
        return workers.size();
    }



    /*
        加载 NNUE 网络：分发给全部工作器；
        之后 set_threads 新建的工作器自动重载
    */

    bool load_nnue(const std::string& file)
    {

        if(workers.empty())
            ensure_workers(1);


        /*
            先用 worker 0 加载：若发生回退，
            将 nnueFile 更新为实际加载成功的
            文件，之后 set_threads 新建的工作器
            直接加载实际文件，避免重复回退警告
        */

        bool ok =
            workers[0]->nnue.load(file);


        nnueFile =
            ok
                ? workers[0]->nnue.loaded_file()
                : file;


        for(size_t i = 1;
            i < workers.size();
            ++i)
        {
            ok =
                workers[i]->nnue.load(nnueFile)
                || ok;
        }


        return ok;

    }



    /*
        加载内嵌于可执行文件的 NNUE 网络：
        分发到全部工作器，并记住本次为内嵌加载，
        之后 set_threads 新建的工作器同样从内存镜像载入
    */

    bool load_nnue_embedded(const std::string& label)
    {

        if(workers.empty())
            ensure_workers(1);


        embeddedNNUE = true;


        bool ok =
            workers[0]->nnue.load_embedded(label);


        nnueFile = label;


        for(size_t i = 1;
            i < workers.size();
            ++i)
        {
            ok =
                workers[i]->nnue.load_embedded(nnueFile)
                || ok;
        }


        return ok;

    }



    bool nnue_loaded() const
    {

        return !workers.empty()
            && workers[0]->nnue.is_loaded();

    }



    /*
        设置线程数：销毁旧工作组释放资源
        （含各工作器的 NNUE 镜像与累加器栈），
        重建后重新加载网络
    */

    void set_threads(size_t n)
    {

        if(n < 1)
            n = 1;


        if(n > 512)
            n = 512;


        if(n == workers.size())
            return;


        workers.clear();


        ensure_workers(n);


        if(!nnueFile.empty())
        {
            if(embeddedNNUE)
                for(auto& w : workers)
                    w->nnue.load_embedded(nnueFile);
            else
                for(auto& w : workers)
                    w->nnue.load(nnueFile);
        }

    }



    /*
        ucinewgame 清理：清空转表
        与全部工作器的搜索状态
    */

    void clear()
    {

        TT.clear();


        for(auto& w : workers)
            w->resetSearchState();

    }



    /*
        线程绑定信息：Salmonia 不做
        NUMA/处理器核绑定，线程由 OS 统一调度
    */

    std::string thread_binding_information_as_string() const
    {

        return
            "none (no NUMA binding; OS default scheduling)";

    }



    // 聚合全部线程的节点数

    uint64_t nodes_searched() const
    {

        uint64_t sum = 0;


        for(auto& w : workers)
            sum += w->nodes;


        return sum;

    }



    /*
        搜索入口：主线程协调并执行搜索，
        辅助线程并行；搜索完成后等待所有
        辅助线程结束再返回最优结果
    */

    int searchBestMove(
        chess::Board& board,
        int maxDepth,
        chess::Move& bestMove
    )
    {

        stopSearch = false;


        if(workers.empty())
            ensure_workers(1);



        /*
            单线程：与旧版完全一致的行为
        */

        if(workers.size() == 1)
        {

            int s =
                workerSearchRoot(
                    *workers[0],
                    board,
                    maxDepth
                );


            bestMove =
                workers[0]->bestMove;


            nodes =
                workers[0]->nodes;


            completedDepth =
                workers[0]->completedDepth;


            return s;

        }



        // ---------------- 多线程 Lazy SMP ----------------

        std::vector<std::thread> helpers;


        for(size_t i = 1;
            i < workers.size();
            ++i)
        {

            /*
                辅助线程的 Board 副本必须在
                创建线程前完成：主线程随后
                会立即在原 board 上走棋
            */

            chess::Board copy = board;


            helpers.emplace_back(
                [this, i, copy, maxDepth]() mutable
                {
                    workerSearchRoot(
                        *workers[i],
                        copy,
                        maxDepth
                    );
                }
            );

        }



        int s =
            workerSearchRoot(
                *workers[0],
                board,
                maxDepth
            );



        /*
            通知仍在搜索的辅助线程退出，
            等待全部辅助线程结束再返回最佳着法
        */

        stopSearch = true;


        for(auto& t : helpers)
            t.join();



        // 择优：完成深度优先，其次分数

        SearchWorker* best =
            workers[0].get();


        for(size_t i = 1;
            i < workers.size();
            ++i)
        {

            SearchWorker* w =
                workers[i].get();


            if(w->completedDepth
                > best->completedDepth
               ||
               (w->completedDepth
                    == best->completedDepth
                && w->score > best->score))
                best = w;

        }



        bestMove =
            best->bestMove;


        s = best->score;


        nodes =
            nodes_searched();


        completedDepth =
            best->completedDepth;


        return s;

    }



private:

    void ensure_workers(size_t n)
    {

        while(workers.size() < n)
        {

            auto w =
                std::make_unique<SearchWorker>();


            w->idx =
                int(workers.size());


            workers.push_back(
                std::move(w));

        }

    }



    std::vector<std::unique_ptr<SearchWorker>>
        workers;


    std::string nnueFile;

    // 网络来源标记：
    // true  = 内嵌于可执行文件，false = 磁盘文件

    bool embeddedNNUE = false;

};




/*
    兼容入口：test/bench 工具仍使用此函数，
    内部默认单线程线程池（不加载 NNUE，
    回退手写评估，与旧版行为一致）
*/

inline int searchBestMove(
    chess::Board& board,
    int maxDepth,
    chess::Move& bestMove
)
{

    static ThreadPool pool;


    return pool.searchBestMove(
        board,
        maxDepth,
        bestMove
    );

}



}


#endif