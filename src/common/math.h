#pragma once

#include "../global.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <numeric>

namespace MathUtils {

  // inplace cholesky
  // matrix must be positive definite and symmetric
  class Cholesky {
  public:
    static constexpr double ftol = 1E-8;

    explicit Cholesky(std::int32_t n): n(n), G(n, vec1D(n)) {}

    std::int32_t Factor(const vec2D& matrix, const double nu) {
      for(std::int32_t i = 0; i < n; i++) { // copy lower triangular matrix
        std::copy_n(begin(matrix[i]), i + 1, begin(G[i]));
      }

      for(std::int32_t i = 0; i < n; i++) {
        // off-diagonal
        for(std::int32_t j = 0; j < i; j++) {
          double sum = G[i][j];
          for(std::int32_t k = 0; k < j; k++) { sum -= (G[i][k] * G[j][k]); }
          G[i][j] = sum / G[j][j];
        }

        // diagonal
        double sum = G[i][i] + nu; // add regularization
        for(std::int32_t k = 0; k < i; k++) { sum -= (G[i][k] * G[i][k]); }
        if(sum > ftol) {
          G[i][i] = std::sqrt(sum);
        } else {
          return 1;
        }
      }
      return 0;
    }

    void Solve(const vec1D& b, vec1D& x) {
      for(std::int32_t i = 0; i < n; i++) {
        double sum = b[i];
        for(std::int32_t j = 0; j < i; j++) { sum -= (G[i][j] * x[j]); }
        x[i] = sum / G[i][i];
      }
      for(std::int32_t i = n - 1; i >= 0; i--) {
        double sum = x[i];
        for(std::int32_t j = i + 1; j < n; j++) { sum -= (G[j][i] * x[j]); }
        x[i] = sum / G[i][i];
      }
    }

    std::int32_t n;
    vec2D G;
  };

  OPTIMIZE_ON
  inline double dot_scalar(const span_cf64& v1, const span_cf64& v2) {
    if(v1.size() != v2.size()) {
      throw std::invalid_argument("invalid_argument");
    }
    return std::transform_reduce(
      v1.begin(), v1.end(), v2.begin(), 0.0, std::plus<>(), std::multiplies<>()
    );
  }

  OPTIMIZE_OFF

  // vector = matrix * vector
  inline vec1D mul(const vec2D& m, const vec1D& v) {
    vec1D v_out(m.size());
    for(std::size_t i = 0; i < m.size(); i++) {
      v_out[i] = MathUtils::dot_scalar(m[i], v);
    }
    return v_out;
  }

  // vector = scalar * vector
  inline vec1D mul(const double s, const vec1D& v) {
    vec1D v_out(v.size());
    for(std::size_t i = 0; i < v.size(); i++) { v_out[i] = s * v[i]; }
    return v_out;
  }

  // matrix = matrix  * matrix
  inline vec2D mul(const vec2D& m1, const vec2D& m2) {
    vec2D m_out(m1.size(), vec1D(m2[0].size()));
    for(std::int32_t j = 0; j < static_cast<std::int32_t>(m_out.size()); j++) {
      for(std::int32_t i = 0; i < static_cast<std::int32_t>(m_out[0].size());
          i++) {
        double sum = 0;
        for(std::int32_t k = 0; k < static_cast<std::int32_t>(m2.size()); k++) {
          sum += m1[j][k] * m2[k][i];
        }
        m_out[j][i] = sum;
      }
    }
    return m_out;
  }

  // vector s1*v1 + s2*v2
  inline vec1D mul_add(double s1, const vec1D& v1, double s2, const vec1D& v2) {
    assert(v1.size() == v2.size());
    vec1D v_out(v1.size());
    for(std::size_t i = 0; i < v1.size(); i++) {
      v_out[i] = s1 * v1[i] + s2 * v2[i];
    }
    return v_out;
  }

  // matrix s1*m1 + s2*m2
  inline vec2D mul_add(double s1, const vec2D& m1, double s2, const vec2D& m2) {
    if(m1.size() == m2.size()) {
      throw std::invalid_argument("invalid_argument");
    }
    vec2D m_out(m1.size());
    for(std::size_t j = 0; j < m1.size(); j++) {
      m_out[j] = mul_add(s1, m1[j], s2, m2[j]);
    }
    return m_out;
  }

  // outer product of u*v^T
  inline vec2D outer(const vec1D& u, const vec1D& v) {
    std::size_t nrows = u.size();
    std::size_t ncols = v.size();
    vec2D m_out(nrows, vec1D(ncols));
    for(std::size_t j = 0; j < nrows; j++) {
      for(std::size_t i = 0; i < ncols; i++) { m_out[j][i] = u[j] * v[i]; }
    }
    return m_out;
  }

  OPTIMIZE_ON
  inline double calc_spow(const span_cf64& x, const span_cf64& powtab) {
    return std::transform_reduce(
      x.begin(), x.end(), powtab.begin(), 0.0, std::plus<>(),
      [](double xi, double pi) { return pi * (xi * xi); }
    );
  }

  OPTIMIZE_OFF

  inline double calc_loglik_L1(double abs_e, double b) {
    return -std::log(2 * b) - abs_e / b;
  }

  inline double calc_loglik_L2(double sq_e, double sigma2) {
    return -0.5 * std::log(2 * std::numbers::pi_v<long double> * sigma2)
           - 0.5 * sq_e / sigma2;
  }

  // inverse of pos. def. symmetric matrix
  class InverseSym {
  public:
    explicit InverseSym(std::int32_t n): chol(n), n(n), b(n) {}

    void Solve(const vec2D& matrix, vec2D& sol, const double nu = 0.0) {
      if(chol.Factor(matrix, nu) == 0) {
        for(std::int32_t i = 0; i < n; i++) {
          std::fill(std::begin(b), std::end(b), 0.0);
          b[i] = 1.0;
          chol.Solve(b, sol[i]);
        }
      };
    }

  protected:
    MathUtils::Cholesky chol;
    std::int32_t n;
    vec1D b;
  };

  // estimate running covariance of vectors of len n
  class EstCov {
  public:
    explicit EstCov(
      std::int32_t n, double alpha = 0.998, double init_val = 1.0
    ):
      mcov(n, vec1D(n)),
      n(n),
      alpha(alpha) {
      for(std::int32_t i = 0; i < n; i++) { mcov[i][i] = init_val; }
    }

    void Update(const vec1D& x) {
      for(std::int32_t j = 0; j < n; j++) {
        for(std::int32_t i = 0; i < n; i++) {
          mcov[j][i] = alpha * mcov[j][i] + (1.0 - alpha) * x[i] * x[j];
        }
      }
    }

    vec2D mcov;
    std::int32_t n;
    double alpha;
  };

  template<typename T> T med3(T a, T b, T c) {
    if((a < b && b < c) || (c < b && b < a)) { return b; }
    if((b < a && a < c) || (c < a && a < b)) { return a; }
    return c;
  }

  inline double
  SumDiff(const std::vector<double>& v1, const std::vector<double>& v2) {
    if(v1.size() != v2.size()) { return -1; }
    double sum = 0.;
    for(std::size_t i = 0; i < v1.size(); i++) { sum += fabs(v1[i] - v2[i]); }
    return sum;
  }

  inline std::int32_t S2U(std::int32_t val) {
    if(val < 0) {
      val = 2 * (-val);
    } else if(val > 0) {
      val = (2 * val) - 1;
    }
    return val;
  }

  inline std::int32_t U2S(std::int32_t val) {
    if((static_cast<std::uint32_t>(val) & 1U) != 0) {
      val =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(val + 1) >> 1U);
    } else {
      val = -static_cast<std::int32_t>(static_cast<std::uint32_t>(val) >> 1U);
    }
    return val;
  }

  inline double
  norm2(const std::vector<double>& vec1, const std::vector<double>& vec2) {
    if(vec1.size() != vec2.size()) { return 0; }
    double sum = 0.;
    for(std::size_t i = 0; i < vec1.size(); i++) {
      double t = vec1[i] - vec2[i];
      sum += t * t;
    };
    return sqrt(sum);
  }

  inline double mean(const std::vector<double>& vec) {
    if(vec.size() != 0U) {
      double sum = std::accumulate(begin(vec), end(vec), 0.0);
      return sum / static_cast<double>(vec.size());
    }
    return 0;
  }

  // contraharmonic mean
  inline double meanL(const std::vector<double>& vec) {
    if(vec.size() != 0U) {
      double sum0 = 0.0;
      double sum1 = 0.0;
      for(std::size_t i = 0; i < vec.size(); ++i) {
        sum0 += (vec[i] * vec[i]);
        sum1 += vec[i];
      }
      if(sum1 > 0.0) { return sum0 / sum1; }
      return 0.;
    }
    return 0.;
  }

  inline double linear_map_n(
    std::int32_t n0, std::int32_t n1, double y0, double y1, std::int32_t idx
  ) {
    auto dx = static_cast<double>(n1 - n0);
    double dy = y1 - y0;
    return idx * (dy / dx) + y0;
  }

  template<typename T> T sgn(T x) {
    return (x > 0) - (x < 0);
    /*if (x>0) return 1;
    if (x<0) return -1;
    return 0;*/
  }
} // namespace MathUtils
