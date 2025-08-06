#pragma once

#include "../common/math.h"
#include "vle.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>

class CostFunction {
public:
  CostFunction() = default;
  CostFunction(const CostFunction&) = default;
  CostFunction(CostFunction&&) = default;
  CostFunction& operator=(const CostFunction&) = default;
  CostFunction& operator=(CostFunction&&) = default;
  virtual ~CostFunction() = default;

  virtual double Calc(span_ci32 buf) const = 0;
};

class CostL1: public CostFunction {
public:
  double Calc(span_ci32 buf) const override {
    if(buf.empty()) { return 0.0; }

    // 使用 std::accumulate 计算绝对值之和
    auto sum = std::accumulate(
      buf.begin(), buf.end(),
      std::int64_t{0}, // 初始化为 std::int64_t 类型以避免溢出
      [](std::int64_t acc, std::int32_t val) { return acc + std::abs(val); }
    );
    return static_cast<double>(sum) / static_cast<double>(buf.size());
  }
};

class CostRMS: public CostFunction {
public:
  double Calc(span_ci32 buf) const override {
    if(buf.size() != 0U) {
      std::int64_t sum = 0.0;
      for(const auto val: buf) { sum += static_cast<std::int64_t>(val) * val; }
      return sqrt(static_cast<double>(sum) / static_cast<double>(buf.size()));
    }
    return 0.;
  }
};

// estimate bytes per frame with a simple golomb model
class CostGolomb: public CostFunction {
  static constexpr double alpha = 0.97; // critical

public:
  CostGolomb() = default;

  double Calc(span_ci32 buf) const override {
    RunWeight rm(alpha);
    if(buf.size() != 0U) {
      std::int64_t nbits = 0;
      for(const auto sval: buf) {
        const std::uint32_t m = std::max(static_cast<std::int32_t>(rm.sum), 1);
        const auto uval = MathUtils::S2U(sval);
        auto q = static_cast<std::int32_t>(uval / m);
        // std::int32_t r=val-q*m;
        nbits += (q + 1);
        if(m > 1) { nbits += std::bit_width(m); }
        rm.Update(uval);
      }
      return static_cast<double>(nbits) / (8.);
    }
    return 0;
  }
};

constexpr bool TOTAL_SELF_INFORMATION = false;

// entropy using order-0 markov model
class CostEntropy: public CostFunction {
public:
  CostEntropy() = default;

  double Calc(span_ci32 buf) const override {
    if(buf.size() == 0U) { return 0; }
    double entropy = 0.0;

    std::int32_t minval = std::numeric_limits<std::int32_t>::max();
    std::int32_t maxval = std::numeric_limits<std::int32_t>::min();
    const auto [min_it, max_it] = std::ranges::minmax_element(buf);
    minval = std::min(minval, *min_it);
    maxval = std::max(maxval, *max_it);
    std::vector<std::int32_t> counts(maxval - minval + 1);

    const auto cmap = [&](std::int32_t val) -> std::int32_t& {
      return counts[val - minval];
    };

    for(const auto val: buf) { ++cmap(val); }

    const double invs = 1.0 / static_cast<double>(buf.size());
    if constexpr(TOTAL_SELF_INFORMATION) {
      for(const auto val: buf) {
        const double p = cmap(val) * invs;
        entropy += p * std::log(p);
      }
    } else {
      if(counts.size() < buf.size()) { // over alphabet
        for(const auto c: counts) {
          if(c == 0) { continue; }
          const double p = c * invs;
          entropy += c * std::log2(p);
        }
      } else { // over input
        for(const auto val: buf) {
          const double p = cmap(val) * invs;
          entropy += std::log2(p);
        }
      }
      entropy = -entropy / 8.0;
    }

    return entropy;
  }
};

/*class StaticBitModel {
  public:
    StaticBitModel()
    :pr(2,vec1D(PSCALE))
    {
      for (std::size_t i=1;i<PSCALE;i++)
      {
        double p1 = static_cast<double>(i)/static_cast<double>(PSCALE);
        double t0 = -std::log2(1.0-p1);
        double t1 = -std::log2(p1);
        pr[0][i]= t0;
        pr[1][i]= t1;
      }
      ResetCount();
    }
    void ResetCount(){nbits=0;};
    void EncodeBitOne(std::uint32_t p1,std::int32_t bit)
    {
      nbits += pr[bit][p1];
    }
    auto EncodeP1_Func() {return [&](std::uint32_t p1,std::int32_t bit) {return
EncodeBitOne(p1,bit);};}; // stupid C++

  double nbits;
  vec2D pr;
};*/

class CostBitplane: public CostFunction {
public:
  CostBitplane() = default;

  double Calc(span_ci32 buf) const override {
    std::size_t numsamples = buf.size();
    std::vector<std::int32_t> ubuf(numsamples);
    std::int32_t vmax = 1;
    for(std::size_t i = 0; i < numsamples; i++) {
      std::int32_t val = MathUtils::S2U(buf[i]);
      vmax = std::max(val, vmax);
      ubuf[i] = val;
    }

    BufIO iobuf;
    RangeCoderSH rc(iobuf);
    rc.Init();
    BitplaneCoder bc_rc(std::ilogb(vmax), numsamples);
    bc_rc.Encode(rc.encode_p1, ubuf.data());
    rc.Stop();
    return static_cast<double>(iobuf.GetBufPos());
  }
};
