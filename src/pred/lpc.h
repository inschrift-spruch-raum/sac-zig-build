#pragma once // LPC_H

#include "../common/utils.h"
#include "../common/math.h"

constexpr bool INIT_COV = false;

class OLS {
  public:
    OLS(int n,int kmax=1,double lambda=0.998,double nu=0.001,double beta_sum=0.6,double beta_pow=0.75,double beta_add=2)
    :x(n),
    chol(n),
    w(n),b(n),mcov(n,vec1D(n)),
    n(n),kmax(kmax),lambda(lambda),nu(n*nu),
    beta_pow(beta_pow),beta_add(beta_add),esum(beta_sum)
    {
      km=0;
      pred=0.0;
      if constexpr (INIT_COV) {
        for (int i=0;i<n;i++) mcov[i][i]=1.0;
      }
    }
    double Predict() {
      pred = slmath::dot_scalar(span_cf64(x.data(), n), span_cf64(w.data(), n));
      return pred;
    }

    void Update(double val)
    {
      // update estimate of covariance matrix
      esum.Update(fabs(val-pred));
      double c0=pow(esum.sum+beta_add,-beta_pow);

      for (int j=0;j<n;j++) {
        // only update lower triangular
        for (int i=0;i<=j;i++) mcov[j][i]=lambda*mcov[j][i]+c0*(x[j]*x[i]);
        b[j]=lambda*b[j]+c0*(x[j]*val);
      }

      km++;
      if (km>=kmax) {
        if (!chol.Factor(mcov,nu)) chol.Solve(b,w);
        km=0;
      }
    }
    vec1D x;
  protected:
    MathUtils::Cholesky chol;
    vec1D w,b;
    vec2D mcov;
    int n,kmax,km;
    double lambda,nu,pred;
    double beta_pow,beta_add;
    RunWeight esum;
};
