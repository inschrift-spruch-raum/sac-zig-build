#ifndef SSE_H
#define SSE_H

#include "counter.h"
#include "domain.h"

template<std::int32_t N> class SSENL {
  // enum {szmap=1<<NB};

public:
  std::int32_t tscale, xscale;
  std::uint16_t p_quant;

  SSENL(std::int32_t scale = myDomain.max):
    tscale(scale),
    xscale((2 * tscale) / (N - 1)) {
    if(xscale == 0) xscale = 1;
    for(std::int32_t i = 0; i <= N; i++) {
      std::int32_t x = myDomain.Inv(i * xscale - tscale);
      Map[0][i].p1 = x;
      Map[1][i].p1 = x;
    }
    lb = 0;
  };

  std::int32_t Predict(std::int32_t p1) {
    std::int32_t pq =
      (std::min)(2 * tscale, (std::max)(0, myDomain.Fwd(p1) + tscale));

    p_quant = pq / xscale;
    std::int32_t p_mod = pq - (p_quant * xscale); //%xscale;

    std::int32_t pl = Map[lb][p_quant].p1;
    std::int32_t ph = Map[lb][p_quant + 1].p1;

    std::int32_t px = (pl * (xscale - p_mod) + ph * p_mod) / xscale;
    return std::clamp(px, 1, PSCALEm);
  };

  void Update(std::int32_t bit, std::int32_t rate, bool updlb = true) {
    Map[lb][p_quant].update(bit, rate);
    Map[lb][p_quant + 1].update(bit, rate);
    if(updlb) lb = bit;
  };

protected:
  LinearCounter16 Map[2][N + 1];
  std::int32_t lb;
};

#endif
