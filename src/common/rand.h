#pragma once

#include <random>

class Random {
  public:
    Random():engine(time(0)){};
    Random(std::uint32_t seed):engine(seed){};
    double r_01() { // [0,1)
      return std::uniform_real_distribution<double>{0,1}(engine);
    };
    double r_01open() { // (0,1)
      return std::uniform_real_distribution<double>{std::nextafter(0.0, std::numeric_limits<double>::max()),1.0}(engine);
    };
    double r_01closed() { // [0,1]
      return std::uniform_real_distribution<double>{0,std::nextafter(1.0, std::numeric_limits<double>::max())}(engine);
    };
    double r_int(double imin,double imax) { //double in [imin,imax]
      return std::uniform_real_distribution<double>{imin,std::nextafter(imax, std::numeric_limits<double>::max())}(engine);
    };
    std::uint32_t ru_int(std::uint32_t imin,std::uint32_t imax) { //std::int32_t in [imin,imax]
      return std::uniform_int_distribution<std::uint32_t>{imin, imax}(engine);
    };
    double r_norm(double mu=0.0,double sigma=1.0) { // normal
      return std::normal_distribution<double>{mu,sigma}(engine);
    }
    double r_cauchy(double mu,double sigma) { // normal
      return std::cauchy_distribution<double>{mu,sigma}(engine);
    }
    double r_lognorm(double mu,double sigma) { // log-normal
      return std::exp(std::normal_distribution<double>{mu,sigma}(engine));
    }
    std::uint32_t ru_geo(double p) { // geometric
      return std::geometric_distribution<std::uint32_t>{p}(engine);
    }
    std::uint32_t ru_poi(double lambda) { // poisson
      return std::poisson_distribution<std::uint32_t>{lambda}(engine);
    }
    bool event(double p) {
      if (r_01()<p) return true;
      else return false;
    };
  private:
    std::mt19937 engine;
};
