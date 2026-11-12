#pragma once

#include <iostream>

#define ReCa reinterpret_cast
#define StCa static_cast
#define CoCa const_cast

using ulong32_t = unsigned long;

#ifndef NDEBUG
#define LOG( ... ) std::cerr << std::format(__VA_ARGS__) << '\n'
#else
#define LOG( ... )
#endif