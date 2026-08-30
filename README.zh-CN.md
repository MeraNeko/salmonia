# Salmonia

一款基于 [Stockfish](https://github.com/official-stockfish/Stockfish) 源码构建的 UCI 国际象棋引擎，具备自定义搜索算法、评估函数、NNUE 集成以及 Polyglot 开局库支持。

## 特性

- **UCI 协议** — 完整兼容通用国际象棋接口（Universal Chess Interface）
- **迭代加深搜索** — PVS（主要变例搜索）配合 Alpha-Beta 剪枝、LMR（晚着法削减）、NMP（空着剪枝）
- **Lazy SMP** — 多线程并行搜索
- **NNUE 评估** — Stockfish SFNNv6 网络集成，含 C++ 回退机制
- **Polyglot 开局库** — 内置开局库支持（`.bin` 格式）
- **单文件分发** — `Salmonia_lite` 目标通过 `incbin` 将 NNUE 权重和开局库直接嵌入可执行文件
- **时间管理** — 高级时间分配策略

## 构建要求

- **C++20** 兼容编译器（GCC 11+、Clang 15+、MSVC 2022+）
- **CMake** >= 3.20
- **Ninja**（推荐）或任意 CMake 支持的构建系统
- 无外部库依赖 — 所有代码自包含

## 构建

### 标准构建

```bash
cmake -S . -B build -G Ninja
cmake --build build --config Release
```

生成以下目标：
- `Salmonia` — 主引擎（运行时需在工作目录放置 `salmonia_30m.nnue`）
- `nnue_test` — NNUE 集成验证工具
- `bench_search` — 搜索基准测试工具
- `test_correctness` — 搜索核心逻辑正确性测试

### 轻量版构建（单文件，仅限 GCC/Clang）

```bash
cmake -S . -B build -G Ninja
cmake --build build --config Release --target Salmonia_lite
```

`Salmonia_lite` 将 NNUE 网络（`salmonia_30m.nnue`）和开局库（`opening/gm2001.bin`）直接嵌入可执行文件 — 运行时无需外部数据文件。

> **注意：** `Salmonia_lite` 需要 GCC 或 Clang（使用 GNU 内联汇编实现 `incbin`）。MSVC 不可用。

## 运行

```bash
# 标准模式（将 salmonia_30m.nnue 放在可执行文件同级目录）
./Salmonia

# 或使用 Salmonia_lite（无需外部文件）
./salmonia_lite
```

引擎通过 UCI 协议通信。输入 `uci` 启动，`quit` 退出。

## 项目结构

```
├── main.cpp                  # UCI 主循环
├── chess.hpp                 # 国际象棋库（Disservin/chess-library v0.9.2）
├── evaluate.hpp              # 静态评估
├── search.hpp                # 搜索算法（PVS + LMR + NMP）
├── time_manager.hpp          # 时间管理
├── transposition.hpp         # 置换表
├── nnue_wrapper.hpp          # NNUE 评估封装
├── polyglot.hpp              # Polyglot 哈希
├── polyglot_book.hpp         # 开局库读取器
├── polyglot_random.inc       # Polyglot 随机表
├── embedded_data.cpp/hpp     # incbin 嵌入数据访问
├── salmonia_embedded_paths.h.in  # CMake 模板，用于嵌入文件路径
├── salmonia_30m.nnue         # 自定义 NNUE 网络
├── opening/gm2001.bin        # 开局库
├── stockfish/src/            # Stockfish 源码
│   ├── nnue/                 # NNUE 架构
│   ├── syzygy/               # Syzygy 残局库探测
│   └── incbin/               # 二进制嵌入工具
└── CMakeLists.txt            # 构建配置
```

## 许可证

本项目采用 GNU General Public License v3.0 许可。详见 [LICENSE](LICENSE)。

本项目整合了以下项目的代码：
- **Stockfish** — GNU GPL v3+（见 `stockfish/src/`）
- **chess-library** — MIT License（Disservin/chess-library）
- **incbin** — Unlicense（见 `stockfish/src/incbin/UNLICENCE`）

由于 Stockfish 的 copyleft 许可证要求，合并后的作品以 GNU General Public License v3.0 发布。
