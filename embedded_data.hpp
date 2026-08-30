#ifndef SALMONIA_EMBEDDED_DATA_HPP
#define SALMONIA_EMBEDDED_DATA_HPP

/*
    单文件分发：读取嵌入在可执行文件只读节中的数据

    符号由 embedded_data.cpp 通过 incbin(.incbin) 定义，
    全局命名空间的 C++ 变量名不参与 name mangling，
    因此这里的 extern 声明与汇编标签一一对应。

    仅在 SALMONIA_LITE 目标下使用。
*/

#include <cstddef>


extern const unsigned char gSalmoniaBookData[];
extern const unsigned int  gSalmoniaBookSize;


namespace salmonia_embedded
{

inline const unsigned char* book_data()
{
    return gSalmoniaBookData;
}

inline std::size_t book_size()
{
    return static_cast<std::size_t>(gSalmoniaBookSize);
}

}

#endif
