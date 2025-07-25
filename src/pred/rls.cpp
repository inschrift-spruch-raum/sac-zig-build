#include "rls.h"
#include "../common/math.h"
#include "../common/utils.h"

constexpr std::int32_t RLS_ALC=1;

RLS::RLS(std::int32_t n,double gamma,double nu)
:n(n),
px(0.),gamma(gamma),
hist(n),w(n),
P(n,vec1D(n)), // inverse covariance matrix
alc(gamma)
{
  for (std::int32_t i=0;i<n;i++)
    P[i][i]=1.0/nu;
}

double RLS::Predict()
{
  px=MathUtils::dot_scalar(hist,w);
  return px;
}

double RLS::Predict(const vec1D &input)
{
  hist=input;
  return Predict();
}

void RLS::Update(double val)
{
  const double err=val-px;

  vec1D ph=MathUtils::mul(P,hist); //phi=hist P hist
  // a priori variance of prediction
  double phi=MathUtils::dot_scalar(hist,ph);

  double alpha=gamma;
  if constexpr(SACGlobalCfg::RLS_ALC) {
    // Normalized Innovation Squared
    // quantifies how "unexpected" the observation is
    // relative to the models uncertainty phi
    double metric = (err*err);//(phi+1E-3);
    alpha=alc.update(metric);
  };

  //update inverse of covariance matrix
  //P(n)=1/lambda*P(n-1)-1/lambda * k(n)*x^T(n)*P(n-1)
  double denom=1./(alpha+phi);
  double inv_alpha=1.0/(alpha);
  for (std::int32_t i=0;i<n;i++)
    for (std::int32_t j=0;j<=i;j++) {
      double m=ph[i]*ph[j]; // outer product of ph
      double v=(P[i][j] - denom * m) * inv_alpha;
      P[i][j] = P[j][i] = v;
    }

  // update weights
  for (std::int32_t i=0;i<n;i++)
      w[i]+=err*(denom*ph[i]);
}

void RLS::UpdateHist(double val)
{
  Update(val);
  miscUtils::RollBack(hist,val);
}
