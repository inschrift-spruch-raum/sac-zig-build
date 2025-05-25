#pragma once // LMS_CASCADE_H

#include "lms.h"
#include "rls.h"

constexpr bool LMS_ADA_CASCADE = false;
constexpr bool LMS_INIT = true;
constexpr bool LMS_CLAMPW = true;

class LMSCascade {
  public:
    LMSCascade(const std::vector<int> &vn,const std::vector<double>&vmu,const std::vector<double>&vmudecay,const std::vector<double> &vpowdecay,double mu_mix,double mu_mix_beta,int lm_n,double lm_alpha)
    :n(vn.size()),
    p(n+1),
    lms_mix(n+1,mu_mix,mu_mix_beta),
    lm(lm_n,lm_alpha),
    clms(n)
    {
      if constexpr (LMS_INIT) {
        for (int i=0;i<n;i++) lms_mix.w[i] = 1.0/(i+1);
      }
      if constexpr (LMS_ADA_CASCADE) {
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
      p[n]=lm.Predict();
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

      lm.UpdateHist(t);
    }
    ~LMSCascade()
    {
      for (int i=0;i<n;i++) delete clms[i];
    }
  private:
    int n;
    vec1D p;
    LAD_ADA lms_mix;
    RLS lm;
    std::vector<LS_Stream*> clms;
};
