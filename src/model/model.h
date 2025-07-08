#pragma once // MODEL_H

#include <cstdint>

// probability precision
constexpr std::int32_t PBITS = 15;
constexpr std::int32_t PSCALE = 1 << PBITS;
constexpr std::int32_t PSCALEh = PSCALE >> 1;
constexpr std::int32_t PSCALEm = PSCALE - 1;

// weight precision
constexpr std::int32_t WBITS = 16;
constexpr std::int32_t WSCALE = 1 << WBITS;
constexpr std::int32_t WSCALEh = WSCALE >> 1;
