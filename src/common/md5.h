#pragma once

#include <cstddef>
#include <cstdint>

namespace MD5 {
  struct MD5Context{
    std::uint64_t size;        // Size of input in bytes
    std::uint32_t buffer[4];   // Current accumulation of hash
    std::uint8_t input[64];    // Input to be used in the next step
    std::uint8_t digest[16];   // Result of algorithm
  };

  std::uint32_t rotateLeft(std::uint32_t x, std::uint32_t n); //Rotates a 32-bit word left by n bits
  void Init(MD5Context *ctx);
  void Update(MD5Context *ctx, std::uint8_t *input, std::size_t input_len);
  void Finalize(MD5Context *ctx);
  void Step(std::uint32_t *buffer, std::uint32_t *input);
};
