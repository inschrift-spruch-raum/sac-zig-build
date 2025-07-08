#ifndef PRED_H
#define PRED_H

#include "../pred/lms.h"
#include "../pred/lms_cascade.h"
#include "../pred/lpc.h"
#include "../pred/bias.h"

class Predictor {
  public:
    struct tparam {
      std::int32_t nA,nB,nM0,nS0,nS1,k;
      std::vector <std::int32_t>vn0,vn1;
      std::vector <double>vmu0,vmu1;
      std::vector <double>vmudecay0,vmudecay1;
      std::vector <double>vpowdecay0,vpowdecay1;
      double lambda0,lambda1,ols_nu0,ols_nu1,mu_mix0,mu_mix1,mu_mix_beta0,mu_mix_beta1;
      double beta_sum0,beta_pow0,beta_add0;
      double beta_sum1,beta_pow1,beta_add1;
      std::int32_t ch_ref;
      double bias_mu0,bias_mu1;
      std::int32_t bias_scale0,bias_scale1;
      std::int32_t lm_n;
      double lm_alpha;
    };
    explicit Predictor(const tparam &p);

    double predict(std::int32_t ch);
    void update(std::int32_t ch,double val);

    void fillbuf_ch0(const std::int32_t *src0,std::int32_t idx0,const std::int32_t *src1,std::int32_t idx1);
    void fillbuf_ch1(const std::int32_t *src0,const std::int32_t *src1,std::int32_t idx1,std::int32_t numsamples);

    tparam p;
    std::int32_t nA,nB,nM0,nS0,nS1;

    OLS ols[2];
    Cascade lms[2];
    BiasEstimator be[2];
    double p_lpc[2],p_lms[2];
};

#endif // PRED_H
