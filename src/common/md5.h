#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace MD5 {
  struct MD5Context{
    std::uint64_t size;        // Size of input in bytes
    std::array<std::uint32_t, 4> buffer;   // Current accumulation of hash
    std::array<std::uint8_t, 64> input;    // Input to be used in the next step
    std::array<std::uint8_t, 16> digest;   // Result of algorithm
  };

  std::uint32_t rotateLeft(std::uint32_t x, std::uint32_t n); //Rotates a 32-bit word left by n bits
  void Init(MD5Context *ctx);
  void Update(MD5Context *ctx,const std::span<const std::uint8_t> &input, std::size_t input_len);
  void Finalize(MD5Context *ctx);
  void Step(std::array<std::uint32_t, 4> &buffer, std::array<std::uint32_t, 16> &input);
}  // namespace MD5
