#ifndef MATH_H
#define MATH_H

#include "../global.h"
#include <cassert>
#include <numeric>

namespace slmath
{

  inline double dot_scalar(const span_cf64 &v1,const span_cf64 &v2) {
    if (v1.size() != v2.size()) throw std::invalid_argument("invalid_argument");
    return std::transform_reduce(
        v1.begin(), v1.end(),
        v2.begin(),
        0.0,
        std::plus<>(),
        std::multiplies<>()
    );
  }

  // vector = matrix * vector
  inline vec1D mul(const vec2D &m,const vec1D &v)
  {
    vec1D v_out(m.size());
    for (std::size_t i=0;i<m.size();i++)
      v_out[i]=slmath::dot_scalar(m[i],v);
    return v_out;
  }

  // vector = scalar * vector
  inline vec1D mul(const double s,const vec1D &v)
  {
    vec1D v_out(v.size());
    for (std::size_t i=0;i<v.size();i++)
      v_out[i]=s*v[i];
    return v_out;
  }

  // matrix = matrix  * matrix
  inline vec2D mul(const vec2D &m1, const vec2D &m2)
  {
    vec2D m_out(m1.size(), vec1D(m2[0].size()));
    for (int j=0;j<(int)m_out.size();j++)
      for (int i=0;i<(int)m_out[0].size();i++)
      {
        double sum=0;
        for (int k=0;k<(int)m2.size();k++)
          sum += m1[j][k]*m2[k][i];
        m_out[j][i] = sum;
      }
    return m_out;
  }

  // outer product of u*v^T
  inline vec2D outer(const vec1D &u,const vec1D &v)
  {
    int nrows=u.size();
    int ncols=v.size();
    vec2D m_out(nrows, vec1D(ncols));
    for (int j=0;j<nrows;j++)
      for (int i=0;i<ncols;i++)
        m_out[j][i]=u[j]*v[i];
    return m_out;
  }

};

#endif

