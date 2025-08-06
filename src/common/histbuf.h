#pragma once // HISTBUF_H

#include "alignbuf.h"

#include <span>
#include <vector>

// circulating buffer, bi-partit
// operator [] starts from 0=newest to oldest
template<typename T> class RollBuffer2 {
public:
  explicit RollBuffer2(std::size_t capacity): n(capacity), buf(2 * capacity) {}

  void push(T val) {
    pos = (pos + n - 1) % n;
    buf[pos] = val;
    buf[pos + n] = val;
  }

  const T& operator[](std::int32_t index) const { return buf[pos + index]; }

  std::span<const T> get_span() const {
    return std::span<const T>{buf.data() + pos, n};
  }

  const T* data() const { return buf.data() + pos; }

private:
  std::size_t n, pos{0};
  std::vector<T, align_alloc<T>> buf;
};
