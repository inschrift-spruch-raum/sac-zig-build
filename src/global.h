#pragma once

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>

using vec1D = std::vector<double>;
using vec2D = std::vector<std::vector<double>>;
using ptr_vec1D = std::vector<double*>;
using span_i32 = std::span<int32_t>;
using span_ci32 = std::span<const int32_t>;
using span_cf64 = std::span<const double>;

#ifndef M_PI
constexpr long double M_PI = 3.14159265358979323846264338327950288;
#endif

constexpr bool UNROLL_AVX256 = false;

constexpr std::string_view SAC_VERSION = "0.7.22";

#define TOSTRING_HELPER(x) #x
#define TOSTRING(x) TOSTRING_HELPER(x)

#ifdef __clang__
constexpr std::string_view COMPILER = "Clang " TOSTRING(__clang_major__
) "." TOSTRING(__clang_minor__) "." TOSTRING(__clang_patchlevel__);
#define OPTIMIZE_ON  _Pragma("GCC optimize(\"Ofast\")")
#define OPTIMIZE_OFF _Pragma("GCC reset_options")
#elifdef __GNUC__
constexpr std::string_view COMPILER = "GCC " TOSTRING(__GNUC__
) "." TOSTRING(__GNUC_MINOR__) "." TOSTRING(__GNUC_PATCHLEVEL__);
#define OPTIMIZE_ON  _Pragma("GCC optimize(\"Ofast\")")
#define OPTIMIZE_OFF _Pragma("GCC reset_options")
#elifdef _MSC_VER
constexpr std::string_view COMPILER = "MSVC ";
#define OPTIMIZE_ON  __pragma(optimize("Ofast", on))
#define OPTIMIZE_OFF __pragma(optimize("", off))
#else
constexpr std::string_view COMPILER = "Unknown";
#define OPTIMIZE_ON
#define OPTIMIZE_OFF
#endif

#ifdef __x86_64__
constexpr std::string_view ARCHITECTURE = "x86_64";
#elifdef __i386__
constexpr std::string_view ARCHITECTURE = "x86_32";
#else
constexpr std::string_view ARCHITECTURE = "Unknown";
#endif

#ifdef __AVX512F__
constexpr std::string_view AVX_STATE = "AVX-512";
#elifdef __AVX2__
constexpr std::string_view AVX_STATE = "AVX2";
#elifdef __AVX__
constexpr std::string_view AVX_STATE = "AVX";
#else
constexpr std::string_view AVX_STATE = "OFF";
#endif