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
using span_i32 = std::span<std::int32_t>;
using span_ci32 = std::span<const std::int32_t>;
using span_cf64 = std::span<const double>;

constexpr bool UNROLL_AVX256 = false;

constexpr std::string_view SAC_VERSION = "0.7.22";

#define TOSTRING_HELPER(x) #x
#define TOSTRING(x) TOSTRING_HELPER(x)

struct SACGlobalCfg {
  static constexpr double NLMS_POW_EPS=1.0;
  static constexpr double LMS_ADA_EPS=1E-5;
  static constexpr bool LMS_MIX_INIT=true;// increase stability
  static constexpr bool LMS_MIX_CLAMPW=true;
  static constexpr bool RLS_ALC=true; //adaptive lambda control
};

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