#pragma once // LMS_CASCADE_H

#include "lms.h"
#include "rls.h"
#include "blend.h"
#include "../common/utils.h"

/*
  double e0=std::abs(target-px0);
  double e1=(target-px1)*(target-px1);
  rp0.Update(e0);
  rp1.Update(e1);

  // log-likelihood under source model
  double pl0=MathUtils::calc_loglik_L1(e0,std::max(rp0.sum,1E-8));
  double pl1=MathUtils::calc_loglik_L2(e1,std::max(rp1.sum,1E-8));
  double nbits0 = -pl0 / std::log(2.0);
  double nbits1 = -pl1 / std::log(2.0);

  cv2.Update(nbits0,nbits1);
*/

// increase stability
constexpr bool LMS_MIX_INIT = true;
constexpr bool LMS_MIX_CLAMPW = true;

// Blend 2xLMS-ADA using L1 + L2 loss
// using absolute error as scoring function
class Blend2LMS_L1 {
  public:
    Blend2LMS_L1(std::int32_t n,double lms_mu,double lms_beta,double blend_beta=0.95)
    :n(n),px0(0.0),px1(0.0),
     mix0(n,lms_mu,lms_beta),
     mix1(n,lms_mu,lms_beta),
     cw2(blend_beta)
    {
      if constexpr(LMS_MIX_INIT)
        for (std::int32_t i=0;i<n-1;i++)
          mix0.w[i] = mix1.w[i] = 1.0/(i+1);
    }
    double GetWeight(std::int32_t index) const
    {
      return cw2.Predict(mix0.w[index],mix1.w[index]);
    }
    double Predict(const vec1D &input)
    {
      px0=mix0.Predict(input);
      px1=mix1.Predict(input);
      return cw2.Predict(px0,px1);
    }
    void UpdateMixer(double target)
    {
      mix0.Update(target);
      mix1.Update(target);
      if constexpr(LMS_MIX_CLAMPW)
        for (std::int32_t i=0;i<n;i++) {
          mix0.w[i]=std::max(mix0.w[i],0.0);
          mix1.w[i]=std::max(mix1.w[i],0.0);
        }
    }
    void UpdateBlend(double target)
    {
      double e0=std::abs(target-px0);
      double e1=std::abs(target-px1);
      cw2.Update(e0,e1);
    }
    std::int32_t n;
    double px0,px1;
    LAD_ADA mix0;
    LMS_ADA mix1;
    Blend2 cw2;
};


class Cascade {
  public:
    Cascade(const std::vector<std::int32_t> &vn,const std::vector<double>&vmu,
               const std::vector<double>&vmudecay,const std::vector<double> &vpowdecay,
               double mu_mix,double mu_mix_beta,std::int32_t lm_n,double lm_alpha)
    :n(vn.size()),p(n+1),
     mix(n+1,mu_mix,mu_mix_beta),
     lm(lm_n,lm_alpha),
     clms(n)
    {
      for (std::int32_t i=0;i<n;i++)
        clms[i]=new NLMS_Stream(vn[i],vmu[i],vmudecay[i],vpowdecay[i]);
    }
    double Predict()
    {
      for (std::int32_t i=0;i<n;i++)
          p[i]=clms[i]->Predict();

      p[n]=lm.Predict();
      return mix.Predict(p);
    }
    void Update(const double target)
    {
      mix.UpdateMixer(target);

      double t=target;
      for (std::int32_t i=0;i<n; i++) {
        clms[i]->Update(t);
        t-=mix.GetWeight(i)*p[i];
      }
      lm.UpdateHist(t);
      mix.UpdateBlend(target);
    }
    ~Cascade()
    {
      for (std::int32_t i=0;i<n;i++) delete clms[i];
    }
  private:
    std::int32_t n;
    vec1D p;
    Blend2LMS_L1 mix;
    RLS lm;
    std::vector<LS_Stream*> clms;
};
