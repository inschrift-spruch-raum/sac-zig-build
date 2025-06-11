#pragma once // BIAS_H

#include "../global.h"
#include "../common/utils.h"
#include "lms.h"

constexpr bool BIAS_ROUND_PRED = true;
constexpr int BIAS_MIX_N = 3;
constexpr int BIAS_MIX_NUMCTX = 4;
constexpr int BIAS_MIX_TYPE = 0;  // 0: SSLMS, 1: LAD_ADA, 2: LMS_ADA
constexpr int BIAS_NAVG = 5;
constexpr int BIAS_CLAMPW = 0;

// Helper function to create the appropriate mixer type
static auto createMixer(double lms_mu) {
  if constexpr (BIAS_MIX_TYPE == 0) {
    return SSLMS(BIAS_MIX_N, lms_mu);
  } else if constexpr (BIAS_MIX_TYPE == 1) {
    return LAD_ADA(BIAS_MIX_N, lms_mu, 0.96);
  } else {
    return LMS_ADA(BIAS_MIX_N, lms_mu, 0.965, 0.005);
  }
}

class BiasEstimator {
  class CntAvg {
    struct bias_cnt {
      int cnt;
      double val;
    };
    public:
      CntAvg(int nb_scale=5,int freq0=4)
      :nscale(1<<nb_scale)
      {
         bias.cnt=freq0;
         bias.val=0.0;
      }
      double get() const
      {
        return bias.val/double(bias.cnt);
      }
      void update(double delta) {
        bias.val+=delta;
        bias.cnt++;
        if (bias.cnt>=nscale) {
          bias.val/=2.0;
          bias.cnt>>=1;
        }
      }
    private:
      const int nscale;
      bias_cnt bias;
  };

  public:
    BiasEstimator(double lms_mu=0.003,int nb_scale=5,double nd_sigma=1.5,double nd_lambda=0.998)
    : mix_ada(BIAS_MIX_NUMCTX, createMixer(lms_mu)),
      hist_input(8),hist_delta(8),
      cnt0(1<<6,CntAvg(nb_scale)),
      cnt1(1<<6,CntAvg(nb_scale)),
      cnt2(1<<6,CntAvg(nb_scale)),
      sigma(nd_sigma),
      run_mv(nd_lambda)
    {
      ctx0=ctx1=ctx2=mix_ctx=0;
      px=0.0;
    }

    void CalcContext(double p)
    {
      int b0=hist_input[0]>p?0:1;
      //int b1=hist_input[1]>p?0:1;

      int b2=hist_delta[0]<0?0:1;
      int b3=hist_delta[1]<0?0:1;
      int b4=hist_delta[2]<0?0:1;
      //int b42=hist_delta[3]<0?0:1;
      int b5=hist_delta[1]<hist_delta[0]?0:1;
      int b6=hist_delta[2]<hist_delta[1]?0:1;
      int b7=hist_delta[3]<hist_delta[2]?0:1;
      int b8=hist_delta[4]<hist_delta[3]?0:1;

      int b9=(fabs(hist_delta[0]))>32?0:1;
      int b10=2*hist_input[0]-hist_input[1]>p?0:1;
      int b11=3*hist_input[0]-3*hist_input[1]+hist_input[2]>p?0:1;

      double sum=0;
      for (int i=0;i<BIAS_NAVG;i++)
        sum+=fabs(hist_delta[i]);
      sum /= static_cast<double>(BIAS_NAVG);

      int t=0;
      if (sum>512) t=2;
      //else if (sum>128) t=2;
      else if (sum>32) t=1;

      ctx0=0;
      ctx0+=b0<<0;
      //ctx0+=b1<<1;
      ctx0+=b2<<1;
      ctx0+=b9<<2;
      ctx0+=b10<<3;
      ctx0+=b11<<4;

      ctx1=0;
      ctx1+=b2<<0;
      ctx1+=b3<<1;
      ctx1+=b4<<2;
      //ctx1+=t<<3;

      ctx2=0;
      ctx2+=b5<<0;
      ctx2+=b6<<1;
      ctx2+=b7<<2;
      ctx2+=b8<<3;
      //ctx2+=mix_ctx<<4;

      mix_ctx=t;
    }
    double Predict(double pred)
    {
      px=pred;

      CalcContext(pred);

      vec1D pb(BIAS_MIX_N);
      pb[0]=cnt0[ctx0].get();
      pb[1]=cnt1[ctx1].get();
      pb[2]=cnt2[ctx2].get();
      return pred+mix_ada[mix_ctx].Predict(pb);
    }
    void Update(double val) {
      double delta;
      if constexpr (BIAS_ROUND_PRED) {
        delta=val-std::round(px);
      } else {
        delta=val-px;
      }
      miscUtils::RollBack(hist_input,val);
      miscUtils::RollBack(hist_delta,delta);

      const auto [mean,var] = run_mv.get();

      const double q=sigma*sqrt(var);
      const double lb=mean-q;
      const double ub=mean+q;

      if ( (delta>lb) && (delta<ub))
      {
        cnt0[ctx0].update(delta);
        cnt1[ctx1].update(delta);
        cnt2[ctx2].update(delta);
      }

      run_mv.Update(delta);
      mix_ada[mix_ctx].Update(delta);
      if constexpr(BIAS_CLAMPW == 1) {
        for (int i=0;i<BIAS_MIX_N;i++)
          mix_ada[mix_ctx].w[i] = std::max(mix_ada[mix_ctx].w[i],0.);
      }
    }
  private:
    using MixerType = decltype(createMixer(0.0)); // Deduce the mixer type
    std::vector<MixerType> mix_ada;
    vec1D hist_input,hist_delta;
    int ctx0,ctx1,ctx2,mix_ctx;
    double px;
    //double alpha,p,bias0,bias1,bias2;
    std::vector<CntAvg> cnt0,cnt1,cnt2;
    const double sigma;
    RunMeanVar run_mv;
};
