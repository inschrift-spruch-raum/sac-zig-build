#pragma once

#include <string_view>

constexpr std::string_view SAC_VERSION = "0.7.18";

#define TOSTRING_HELPER(x) #x
#define TOSTRING(x) TOSTRING_HELPER(x)

#ifdef __clang__
#  define COMPILER                                                             \
    "clang " TOSTRING(__clang_major__) "." TOSTRING(__clang_minor__            \
    ) "." TOSTRING(__clang_patchlevel__)
#elifdef __GNUC__
#  define COMPILER                                                             \
    "gcc" TOSTRING(__GNUC__) "." TOSTRING(__GNUC_MINOR__                       \
    ) "." TOSTRING(__GNUC_PATCHLEVEL__)
#else
#  define COMPILER "Unknown"
#endif

#ifdef __x86_64__
#  define ARCHITECTURE "x86_64"
#elifdef __i386__
#  define ARCHITECTURE "x86_32"
#else
#  define ARCHITECTURE "Unknown"
#endif

#ifdef __AVX512F__
#  define AVX_STATE "AVX-512"
#elifdef __AVX2__
#  define AVX_STATE "AVX2"
#elifdef __AVX__
#  define AVX_STATE "AVX"
#else
#  define AVX_STATE "OFF"
#endif