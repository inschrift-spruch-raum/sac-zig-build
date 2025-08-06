#ifndef MIXER_H
#define MIXER_H

#include "domain.h"
#include "model.h"

#include <algorithm>
#include <vector>

// adaptive linear 2-input mix
// maximum weight precision 16-Bit
class Mix2 {
public:
  virtual std::int32_t Predict(std::int32_t _p1, std::int32_t _p2) = 0;
  virtual void Update(std::int32_t bit, std::int32_t rate) = 0;
};

class Mix2Linear: public Mix2 {
public:
  Mix2Linear() { Init(WSCALEh); };

  void Init(std::int32_t iw) { w = iw; };

  // pm=(1-w)*p1+w*p2
  std::int32_t Predict(std::int32_t _p1, std::int32_t _p2) {
    p1 = _p1;
    p2 = _p2;
    pm = p1 + idiv_signed32((p2 - p1) * w, WBITS);
    pm = std::clamp(pm, 1, PSCALEm);
    return pm;
  }

  std::int32_t w, p1, p2, pm;

protected:
  inline std::int32_t idiv_signed32(std::int32_t val, std::int32_t s) {
    return val < 0 ? -(((-val) + (1 << (s - 1))) >> s)
                   : (val + (1 << (s - 1))) >> s;
  };

  inline void upd_w(std::int32_t d, std::int32_t rate) {
    std::int32_t wd = idiv_signed32(rate * d, PBITS);
    w = std::clamp(w + wd, 0, std::int32_t(WSCALE));
  };
};

class Mix2LeastSquares: public Mix2Linear {
public:
  // w_(i+1)=w_i + rate*(p2-p1)*e
  void Update(std::int32_t bit, std::int32_t rate) {
    std::int32_t e = (bit << PBITS) - pm;
    std::int32_t d = idiv_signed32((p2 - p1) * e, PBITS);
    upd_w(d, rate);
  }
};

class Mix2LeastCost: public Mix2Linear {
public:
  void Update(std::int32_t bit, std::int32_t rate) {
    std::int32_t d;
    // if (bit) d=(((p2-p1)<<PBITS)*std::uint64_t(div32tbl[pm]))>>32;
    // else d=(((p1-p2)<<PBITS)*std::uint64_t(div32tbl[PSCALE-pm]))>>32;
    if(bit) d = ((p2 - p1) << PBITS) / pm;
    else d = ((p1 - p2) << PBITS) / (PSCALE - pm);
    upd_w(d, rate);
  }
};

class NMixLogistic {
  enum {
    WRANGE = 1 << 19
  };

  std::vector<std::int16_t> x;
  std::vector<std::int32_t> w;

  std::int16_t pd;
  std::uint8_t n;

public:
  NMixLogistic(std::int32_t n): x(n), w(n), pd(0), n(n) { Init(0); };

  void Init(std::int32_t iw) {
    for(std::int32_t i = 0; i < n; i++) w[i] = iw;
  };

  std::int32_t Predict(const std::vector<std::int32_t>& p) {
    std::int64_t sum = 0;
    for(std::int32_t i = 0; i < n; i++) {
      x[i] = myDomain.Fwd(p[i]);
      sum += std::int64_t(w[i] * x[i]);
    }
    sum = idiv_signed64(sum, WBITS);
    pd = std::clamp(myDomain.Inv(sum), 1, PSCALEm);
    return pd;
  }

  void Update(std::int32_t bit, std::int32_t rate) {
    std::int32_t err = (bit << PBITS) - pd;
    for(std::int32_t i = 0; i < n; i++) {
      std::int32_t de = idiv_signed32(x[i] * err, myDomain.dbits);
      upd_w(i, idiv_signed32(de * rate, myDomain.dbits));
    }
  };

protected:
  inline std::int32_t idiv_signed32(std::int32_t val, std::int32_t s) {
    return val < 0 ? -(((-val) + (1 << (s - 1))) >> s)
                   : (val + (1 << (s - 1))) >> s;
  };

  inline std::int32_t idiv_signed64(std::int64_t val, std::int64_t s) {
    return val < 0 ? -(((-val) + (1 << (s - 1))) >> s)
                   : (val + (1 << (s - 1))) >> s;
  };

  inline void upd_w(std::int32_t i, std::int32_t wd) {
    w[i] = std::clamp(w[i] + wd, -WRANGE, WRANGE - 1);
  }
};

#endif // MIXER_H
