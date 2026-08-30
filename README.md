[![中文](https://img.shields.io/badge/语言-中文-red)](README.zh-CN.md) 
[![English](https://img.shields.io/badge/Language-English-blue)](README.md)
# Salmonia

A UCI chess engine built on top of [Stockfish](https://github.com/official-stockfish/Stockfish) source code, featuring custom search algorithms, evaluation, NNUE integration, and Polyglot opening book support.

## Features

- **UCI Protocol** — Full Universal Chess Interface compatibility
- **Iterative Deepening Search** — PVS with Alpha-Beta pruning, LMR, NMP
- **Lazy SMP** — Multi-threaded parallel search
- **NNUE Evaluation** — Stockfish SFNNv6 network integration with C++ fallback
- **Polyglot Opening Book** — Built-in opening book support (`.bin` format)
- **Single-Binary Distribution** — `Salmonia_lite` target embeds NNUE weights and opening book directly into the executable via `incbin`
- **Time Management** — Advanced time allocation strategy

## Build Requirements

- **C++20** compatible compiler (GCC 11+, Clang 15+, MSVC 2022+)
- **CMake** >= 3.20
- **Ninja** (recommended) or any CMake-supported build system
- No external library dependencies — all code is self-contained

## Building

### Standard build

```bash
cmake -S . -B build -G Ninja
cmake --build build --config Release
```

This produces:
- `Salmonia` — Main engine (requires `salmonia_30m.nnue` in working directory at runtime)
- `nnue_test` — NNUE integration verification tool
- `bench_search` — Search benchmark utility
- `test_correctness` — Search core logic correctness tests

### Lite build (single binary, GCC/Clang only)

```bash
cmake -S . -B build -G Ninja
cmake --build build --config Release --target Salmonia_lite
```

`Salmonia_lite` embeds the NNUE network (`salmonia_30m.nnue`) and opening book (`opening/gm2001.bin`) directly into the executable — no external data files needed at runtime.

> **Note:** `Salmonia_lite` requires GCC or Clang (uses GNU inline assembly for `incbin`). Not available with MSVC.

## Running

```bash
# Standard mode (place salmonia_30m.nnue alongside the executable)
./Salmonia

# Or use Salmonia_lite (no external files needed)
./salmonia_lite
```

The engine communicates via UCI protocol. Type `uci` to start, `quit` to exit.

## Project Structure

```
├── main.cpp                  # UCI main loop
├── chess.hpp                 # Chess library (Disservin/chess-library v0.9.2)
├── evaluate.hpp              # Static evaluation
├── search.hpp                # Search algorithm (PVS + LMR + NMP)
├── time_manager.hpp          # Time management
├── transposition.hpp         # Transposition table
├── nnue_wrapper.hpp          # NNUE evaluation wrapper
├── polyglot.hpp              # Polyglot hashing
├── polyglot_book.hpp         # Opening book reader
├── polyglot_random.inc       # Polyglot random table
├── embedded_data.cpp/hpp     # incbin embedded data access
├── salmonia_embedded_paths.h.in  # CMake template for embedded file paths
├── salmonia_30m.nnue         # Custom NNUE network
├── opening/gm2001.bin        # Opening book
├── stockfish/src/            # Stockfish source code
│   ├── nnue/                 # NNUE architecture
│   ├── syzygy/               # Syzygy tablebase probe
│   └── incbin/               # Binary embedding utility
└── CMakeLists.txt            # Build configuration
```

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.

This project incorporates code from:
- **Stockfish** — GNU GPL v3+ (see `stockfish/src/`)
- **chess-library** — MIT License (Disservin/chess-library)
- **incbin** — Unlicense (see `stockfish/src/incbin/UNLICENCE`)

The combined work is distributed under the GNU General Public License v3.0 due to Stockfish's copyleft license.
