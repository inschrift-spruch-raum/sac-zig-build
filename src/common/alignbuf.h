#pragma once

#include <cstddef>
#include <new>

template<typename T, std::size_t align_t = 64> struct align_alloc {
  using value_type = T;

  template<typename U> struct rebind {
    using other = align_alloc<U, align_t>;
  };

  constexpr align_alloc() noexcept = default;
  constexpr align_alloc(const align_alloc&) noexcept = default;
  constexpr align_alloc(align_alloc&&) noexcept = default;
  align_alloc& operator=(const align_alloc&) noexcept = default;
  align_alloc& operator=(align_alloc&&) noexcept = default;
  ~align_alloc() noexcept = default;

  template<typename U>
  constexpr explicit align_alloc(
    const align_alloc<U, align_t>& /*unused*/
  ) noexcept {}

  T* allocate(std::size_t n) {
    auto ptr =
      static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t(align_t)));
    return ptr;
  }

  void deallocate(T* p, std::size_t n) noexcept {
    ::operator delete(p, n * sizeof(T), std::align_val_t(align_t));
  }
};

template<typename T, typename U, std::size_t align_t>
bool operator==(
  const align_alloc<T, align_t>& /*unused*/,
  const align_alloc<U, align_t>& /*unused*/
) noexcept {
  return true;
}

template<typename T, typename U, std::size_t align_t>
bool operator!=(
  const align_alloc<T, align_t>& /*unused*/,
  const align_alloc<U, align_t>& /*unused*/
) noexcept {
  return false;
}
