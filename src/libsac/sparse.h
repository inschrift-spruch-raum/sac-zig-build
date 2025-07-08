#pragma once // SPARSEPCM_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

#include "../common/utils.h"

class SimplePred {
public:
  SimplePred()= default;

  double Predict() const { return lb; }

  void Update(std::int32_t val) { lb = val; }

protected:
  std::int32_t lb{0};
};

class SparsePCM {
  static constexpr double cost_pow = 1;

public:
  SparsePCM()= default;

  void Analyse(std::span<const std::int32_t> &buf) {
    minval = std::numeric_limits<std::int32_t>::max();
    maxval = std::numeric_limits<std::int32_t>::min();
    for(const auto val: buf) {
      minval = std::min(minval, val);
      maxval = std::max(maxval, val);
    }

    const std::size_t range = maxval - minval + 1;
    used.assign(range, false);
    std::int32_t unique_count = 0;

    for(const auto val: buf) {
      const std::size_t idx = val - minval;
      if(!used[idx]) {
        used[idx] = true;
        ++unique_count;
      }
    }
    fraction_used = (range > 0) ? (unique_count * 100.0 / static_cast<double>(range)) : 0.0;

    prefix_sum.resize(range + 1);
    prefix_sum[0] = 0;
    for(std::size_t i = 0; i < range; ++i) {
      prefix_sum[i + 1] = prefix_sum[i] + static_cast<std::int32_t>(used[i]);
    }

    double sum0 = 0.0;
    double sum1 = 0.0;
    for(const auto val: buf) {
      const std::int32_t e0 = val;
      const std::int32_t e1 = map_val(e0);

      sum0 += std::pow(std::fabs(e0), cost_pow);
      sum1 += std::pow(std::fabs(e1), cost_pow);
    }

    fraction_cost = (sum1 > 0) ? (sum0 / sum1) : 0.0;
  }

  std::int32_t map_val(std::int32_t val, std::int32_t p = 0) const {
    if(val == 0) { return 0;}

    const std::int32_t sgn = MathUtils::sgn(val);
    const std::int32_t pidx=p - minval;

    std::int32_t start = pidx + static_cast<std::int32_t>(val > 0) * 1 + static_cast<std::int32_t>(val < 0) * val;
    std::int32_t end = pidx + static_cast<std::int32_t>(val > 0) * val + static_cast<std::int32_t>(val < 0) * (-1);

    return sgn * (prefix_sum[end + 1] - prefix_sum[start]);
  }

  std::int32_t minval{0}, maxval{0};
  double fraction_used{0.}, fraction_cost{0.};

protected:
  std::vector<bool> used;
  std::vector<std::int32_t> prefix_sum;
};
