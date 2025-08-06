#ifndef PROFILE_H
#define PROFILE_H

#include "map.h"
#include "pred.h"

#include <variant>
#include <vector>

class SACProfile {
public:
  struct FrameStats {
    std::int32_t maxbpn{}, maxbpn_map{};
    bool enc_mapped{};
    std::int32_t blocksize{}, minval{}, maxval{}, mean{};
    Remap mymap;
  };

  struct elem {
    float vmin, vmax;
    std::variant<float, std::uint16_t> val;
  };

  void add_float(float vmin, float vmax, float val) {
    vparam.push_back(elem{vmin, vmax, val});
  }

  float get_float() {
    float val = get<float>(vparam[index].val);
    index++;
    return val;
  }

  void add_ols() {
    add_float(0.99, 0.9999, 0.998); // lambda
    add_float(0.001, 10.0, 0.001);  // ols-nu
    add_float(4, 32, 16);           //
  }

  void get_ols(Predictor::tparam& param) {
    param.lambda0 = get_float();
    param.ols_nu0 = get_float();
    param.nA = get_float();
  }

  void set_profile() { add_ols(); }

  void get_profile(Predictor::tparam& param, bool optimize = false) {
    if(optimize) {
      param.k = 4;
    } else {
      param.k = 1;
    }
    index = 0;
    get_ols(param);
  }

  SACProfile() = default;
  std::vector<elem> vparam;

protected:
  std::int32_t index{};
};

class SacProfile {
public:
  struct FrameStats {
    std::int32_t maxbpn{}, maxbpn_map{};
    bool enc_mapped{};
    std::int32_t blocksize{}, minval{}, maxval{}, mean{};
    Remap mymap;
  };

  struct coef {
    float vmin, vmax, vdef;
  };

  SacProfile() = default;

  void Init(std::int32_t numcoefs) { coefs.resize(numcoefs); }

  explicit SacProfile(std::int32_t numcoefs): coefs(numcoefs) {}

  std::int32_t LoadBaseProfile();

  std::size_t get_size() const { return coefs.size(); };

  void Set(std::int32_t num, double vmin, double vmax, double vdef) {
    if(num >= 0 && num < static_cast<std::int32_t>(coefs.size())) {
      coefs[num].vmin = vmin;
      coefs[num].vmax = vmax;
      coefs[num].vdef = vdef;
    }
  }

  void Set(std::int32_t num, const std::vector<float>& x) {
    if(num >= 0 && num < static_cast<std::int32_t>(coefs.size())
       && (x.size() >= 3)) {
      coefs[num].vmin = x[0];
      coefs[num].vmax = x[1];
      coefs[num].vdef = x[2];
    }
  }

  float Get(std::size_t num) const {
    if(num < coefs.size()) { return coefs[num].vdef; }
    return 0.;
  }

  std::vector<coef> coefs;
};

#endif // PROFILE_H
