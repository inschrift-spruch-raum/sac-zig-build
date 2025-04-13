#pragma once // LMS_CASCADE_H

#include "lms.h"

constexpr bool LMS_ADA = false;
constexpr bool LMS_N0 = true;
constexpr bool LMS_INIT = true;
constexpr bool LMS_CLAMPW = true;

class LMSCascade {
  public:
    LMSCascade(const std::vector<int> &vn,
               const std::vector<double>&vmu,
               const std::vector<double>&vmudecay,
               const std::vector<double> &vpowdecay,
               double mu_mix,
               double mu_mix_beta)
      : n(vn.size()),

    p(LMS_N0 ? n + 1 : n),
    lms_mix(LMS_N0 ? n + 1 : n, mu_mix, mu_mix_beta),
    clms(n)
    {
      if constexpr (LMS_INIT) {
        for (int i=0;i<n;i++) lms_mix.w[i] = 1.0/(i+1);
      }
      if constexpr (LMS_ADA) {
        for (int i=0;i<n-1;i++)
          clms[i]=new NLMS_Stream(vn[i],vmu[i],vmudecay[i],vpowdecay[i]);
        clms[n-1]=new LMSADA_Stream(vn[n-1],vmu[n-1],vmudecay[n-1],vpowdecay[n-1]);
      } else {
        for (int i=0;i<n;i++)
          clms[i]=new NLMS_Stream(vn[i],vmu[i],vmudecay[i],vpowdecay[i]);
      }
    }
    double Predict()
    {
      for (int i=0;i<n;i++) {
          p[i]=clms[i]->Predict();
      }
      return lms_mix.Predict(p);
    }
    void Update(const double target)
    {
      lms_mix.Update(target);

      double t=target;
      for (int i=0;i<n; i++) {
        clms[i]->Update(t);
        if constexpr (LMS_CLAMPW) {
          t-=std::max(lms_mix.w[i],0.0)*p[i];
        } else {
          t-=lms_mix.w[i]*p[i];
        }
      }
      if constexpr (LMS_N0) {
        p[n] = t;
      }
    }
    ~LMSCascade()
    {
      for (int i=0;i<n;i++) delete clms[i];
    }
  private:
    int n;
    vec1D p;
    LAD_ADA lms_mix;
    std::vector<LS_Stream*> clms;
};
