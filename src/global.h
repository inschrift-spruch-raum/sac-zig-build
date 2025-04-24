#pragma once

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <vector>

using vec1D = std::vector<double>;
using vec2D = std::vector<std::vector<double>>;
using ptr_vec1D = std::vector<double*>;
using span_i32 = std::span<int32_t>;
using span_ci32 = std::span<const int32_t>;
using span_f64 = std::span<const double>;

constexpr bool UNROLL_AVX256 = false;
