#pragma once // COUNTER_H

#include "model.h"

#include <algorithm>
#include <array>

class Prob16Counter {
public:
  std::uint16_t p1;
  Prob16Counter(): p1(PSCALEh) {};

protected:
  static std::int32_t idiv(std::int32_t val, std::int32_t s) {
    return (val + (1 << (s - 1))) >> s;
  };

  static std::int32_t idiv_signed(std::int32_t val, std::int32_t s) {
    return val < 0 ? -(((-val) + (1 << (s - 1))) >> s)
                   : (val + (1 << (s - 1))) >> s;
  };
};

// Linear Counter, p=16 bit
class LinearCounter16: public Prob16Counter {
public:
  using Prob16Counter::Prob16Counter;

  // p'=(1-w0)*p+w0*((1-w1)*bit+w1*0.5)
  void update(std::int32_t bit, const std::int32_t w0, const std::int32_t w1) {
    auto wh = [](std::int32_t w) { return ((w * PSCALEh + PSCALEh) >> PBITS); };
    std::int32_t h = (w0 * wh(w1)) >> PBITS;
    std::int32_t p = idiv((PSCALE - w0) * p1, PBITS);
    p += (bit != 0) ? w0 - h : h;
    p1 = std::clamp(p, 1, PSCALEm);
  };

  // p'+=L*(bit-p)
  void update(std::int32_t bit, std::int32_t L) {
    std::int32_t err = (bit << PBITS) - p1;
    // p1 should be converted to "std::int32_t" implicit anyway?
    std::int32_t px =
      static_cast<std::int32_t>(p1) + idiv_signed(L * err, PBITS);
    p1 = std::clamp(px, 1, PSCALEm);
  }
};

static struct tdiv_tbl {
  tdiv_tbl() {
    for(std::int32_t i = 0; i < PSCALE; i++) { tbl[i] = PSCALE / (i + 3); }
  }

  std::int32_t& operator[](std::int32_t i) { return tbl[i]; };

  std::array<std::int32_t, PSCALE> tbl{};
} div_tbl;

class LinearCounterLimit: public Prob16Counter {
  std::uint16_t counter{0};

public:
  LinearCounterLimit() = default;

  void update(std::int32_t bit, std::int32_t limit) {
    if(counter < limit) { counter++; }
    std::int32_t dp = 0;
    dp = (bit != 0) ? ((PSCALE - p1) * div_tbl[counter]) >> PBITS
                    : -((p1 * div_tbl[counter]) >> PBITS);
    // std::int32_t dp=(((bit<<PBITS)-p1)*div_tbl[counter]+PSCALEh)>>PBITS;

    p1 = std::clamp(p1 + dp, 1, PSCALEm);
  };
};
