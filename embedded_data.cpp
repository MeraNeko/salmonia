/*
    单文件分发数据嵌入（仅 SALMONIA_LITE 目标编译本文件）

    使用 Stockfish 自带的 incbin.h（与 network.cpp 嵌入
    默认网络完全相同的机制）把外部数据文件原样嵌入
    可执行文件的只读节：

        EmbeddedNNUE   ->  salmonia_30m.nnue
            定义 gEmbeddedNNUEData / gEmbeddedNNUESize。
            本目标以 -DUNIVERSAL_BINARY 编译，使
            network.cpp 内这两个符号退化为 extern 引用，
            SF 自带的内存流加载路径 load_internal()
            即读取此处数据（零临时文件、零磁盘访问）。

        SalmoniaBook   ->  opening/gm2001.bin
            定义 gSalmoniaBookData / gSalmoniaBookSize，
            由 main.cpp 经 load_from_memory 解析为
            Polyglot 开局库条目。

    注意：incbin 依赖 GNU 内联汇编，仅 GCC/Clang 可用；
    不修改任何搜索与评估逻辑。
*/

#include "salmonia_embedded_paths.h"


#define INCBIN_PREFIX g
#include "incbin/incbin.h"


// NNUE 权重：顶替 SF 默认嵌入网络

INCBIN(EmbeddedNNUE, SALMONIA_EMBEDDED_NNUE_FILE);


// Polyglot 开局库

INCBIN(SalmoniaBook, SALMONIA_EMBEDDED_BOOK_FILE);
