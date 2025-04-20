#pragma once // SPARSEPCM_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

#include "../common/utils.h"

class SimplePred {
public:
  SimplePred(): lb(0) {}

  double Predict() { return lb; }

  void Update(int32_t val) { lb = val; }

protected:
  int32_t lb;
};

class SparsePCM {
  const double cost_pow = 1;

public:
  SparsePCM(): minval(0), maxval(0), fraction_used(0.0), fraction_cost(0.0) {}

  void Analyse(std::span<const int32_t> &buf) {
    minval = std::numeric_limits<int32_t>::max();
    maxval = std::numeric_limits<int32_t>::min();
    for(const auto val: buf) {
      minval = std::min(minval, val);
      maxval = std::max(maxval, val);
    }

    const size_t range = maxval - minval + 1;
    used.assign(range, false);
    int unique_count = 0;

    for(const auto val: buf) {
      const size_t idx = val - minval;
      if(!used[idx]) {
        used[idx] = true;
        ++unique_count;
      }
    }
    fraction_used = (range > 0) ? (unique_count * 100.0 / range) : 0.0;

    prefix_sum.resize(range + 1);
    prefix_sum[0] = 0;
    for(size_t i = 0; i < range; ++i) {
      prefix_sum[i + 1] = prefix_sum[i] + used[i];
    }

    double sum0 = 0.0, sum1 = 0.0;
    for(const auto val: buf) {
      const int32_t e0 = val;
      const int32_t e1 = map_val(e0);

      sum0 += pow(std::fabs(e0), cost_pow);
      sum1 += pow(std::fabs(e1), cost_pow);
    }

    fraction_cost = (sum1 > 0) ? (sum0 / sum1) : 0.0;
  }

  int32_t map_val(int32_t val, int32_t p = 0) const {
    if(val == 0) return 0;

    const int sgn = MathUtils::sgn(val);
    const int32_t pidx=p - minval;

    int start = pidx + (val > 0) * 1 + (val < 0) * val;
    int end = pidx + (val > 0) * val + (val < 0) * (-1);

    return sgn * (prefix_sum[end + 1] - prefix_sum[start]);
  }

  int32_t minval, maxval;
  double fraction_used, fraction_cost;

protected:
  std::vector<bool> used;
  std::vector<int> prefix_sum;
};
