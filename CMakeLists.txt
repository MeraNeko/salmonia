cmake_minimum_required(VERSION 3.20)

project(Salmonia)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 默认 Release 构建
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()


# ============================================================
# Stockfish source
# ============================================================

file(GLOB_RECURSE STOCKFISH_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/stockfish/src/*.cpp"
)

# 删除 Stockfish main（与 Salmonia 主循环冲突）
list(FILTER STOCKFISH_SRC EXCLUDE REGEX ".*/main\\.cpp$")

# 删除 universal（训练 / 格式转换 / 嵌入处理，运行时不需要）
list(FILTER STOCKFISH_SRC EXCLUDE REGEX ".*/universal/.*")


# ============================================================
# include
# ============================================================

set(SALMONIA_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stockfish/src
)


# ============================================================
# executable: Salmonia（主引擎）
# ============================================================

add_executable(Salmonia
    main.cpp
    ${STOCKFISH_SRC}
)

target_include_directories(Salmonia PRIVATE ${SALMONIA_INCLUDE_DIRS})


# ============================================================
# executable: Salmonia_lite（单文件分发版，NNUE/开局库嵌入 exe）
# 仅限 GCC/Clang（依赖 GAS .incbin，MSVC 不可用）
# ============================================================

set(SALMONIA_NNUE_ABS_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/nn-ab28990d4ea3.nnue")

set(SALMONIA_BOOK_ABS_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/opening/gm2001.bin")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/salmonia_embedded_paths.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/salmonia_embedded_paths.h"
    @ONLY
)

if(NOT MSVC)
    add_executable(Salmonia_lite
        main.cpp
        embedded_data.cpp
        ${STOCKFISH_SRC}
    )

    target_include_directories(Salmonia_lite PRIVATE
        ${SALMONIA_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}/generated
    )

    target_compile_definitions(Salmonia_lite PRIVATE
        SALMONIA_LITE
        UNIVERSAL_BINARY
    )

    set_target_properties(Salmonia_lite PROPERTIES
        OUTPUT_NAME "salmonia_lite"
    )

    target_link_options(Salmonia_lite PRIVATE -static)
    target_compile_options(Salmonia_lite PRIVATE -O3 -march=native)

    set_source_files_properties(embedded_data.cpp PROPERTIES
        OBJECT_DEPENDS
        "${SALMONIA_NNUE_ABS_PATH};${SALMONIA_BOOK_ABS_PATH}"
    )
endif()


# ============================================================
# executable: nnue_test（NNUE 集成验证）
# ============================================================

add_executable(nnue_test
    nnue_test.cpp
    ${STOCKFISH_SRC}
)

target_include_directories(nnue_test PRIVATE ${SALMONIA_INCLUDE_DIRS})


# ============================================================
# executable: bench_search（搜索基准测试）
# ============================================================

add_executable(bench_search
    bench_search.cpp
    ${STOCKFISH_SRC}
)

target_include_directories(bench_search PRIVATE ${SALMONIA_INCLUDE_DIRS})


# ============================================================
# executable: test_correctness（搜索核心逻辑功能验证）
# ============================================================

add_executable(test_correctness
    test_correctness.cpp
    ${STOCKFISH_SRC}
)

target_include_directories(test_correctness PRIVATE ${SALMONIA_INCLUDE_DIRS})


# ============================================================
# compiler optimization
# ============================================================

set(ALL_TARGETS Salmonia nnue_test bench_search test_correctness)
if(NOT MSVC)
    list(APPEND ALL_TARGETS Salmonia_lite)
endif()

if(MSVC)
    foreach(tgt ${ALL_TARGETS})
        target_compile_options(${tgt} PRIVATE /O2)
    endforeach()
else()
    foreach(tgt ${ALL_TARGETS})
        target_compile_options(${tgt} PRIVATE -O3 -march=native)
    endforeach()
endif()
