#pragma once

#include "../model/range.h"
#include "../model/counter.h"
#include "../model/sse.h"
#include "../model/mixer.h"
#include "../common/utils.h"
#include <functional>

class StaticLaplaceModel {
  public:
    StaticLaplaceModel(std::int32_t maxbpn)
    :pr((1<<maxbpn),std::vector<std::int32_t>(32))
    {
      for (std::int32_t sum=0;sum<(1<<maxbpn);sum++) {
        for (std::int32_t bpn=0;bpn<32;bpn++) {
          double pd=0.;
          if (sum>0) {
            double theta=std::exp(-1.0/static_cast<double>(sum));
            pd=1.0-1.0/(1+std::pow(theta,1<<bpn));
          }
          std::int32_t pi=std::clamp((std::int32_t)std::round(pd*PSCALE),1,PSCALEm);
          pr[sum][bpn]=pi;
        }
      }
    }
    std::int32_t Predict(std::int32_t avg,std::int32_t bpn)
    {
      return pr[avg][bpn];
    }
  private:
    std::vector<std::vector<std::int32_t>> pr;
};

using EncodeP1 = std::function<void(std::uint32_t,std::int32_t)>;
using DecodeP1 = std::function<std::int32_t(std::uint32_t)>;

class BitplaneCoder {
  const std::int32_t cnt_upd_rate_p=150;
  const std::int32_t cnt_upd_rate_sig=300;
  const std::int32_t cnt_upd_rate_ref=150;
  const std::int32_t mix_upd_rate_ref=800;
  const std::int32_t mix_upd_rate_sig=700;
  const std::int32_t cntsse_upd_rate=250;
  const std::int32_t mixsse_upd_rate=250;
  public:
    BitplaneCoder(std::int32_t maxbpn,std::int32_t numsamples);
    void Encode(EncodeP1 encode_p1,std::int32_t *abuf);
    void Decode(DecodeP1 decode_p1,std::int32_t *buf);
  private:
    void CountSig(std::int32_t n,std::int32_t &n1,std::int32_t &n2);
    void GetSigState(std::int32_t i); // get actual significance state
    std::int32_t PredictLaplace(std::uint32_t avg_sum);
    std::int32_t PredictRef();
    void UpdateRef(std::int32_t bit);
    std::int32_t PredictSig();
    void UpdateSig(std::int32_t bit);
    std::int32_t PredictSSE(std::int32_t p1);
    void UpdateSSE(std::int32_t bit);
    std::uint32_t GetAvgSum(std::int32_t n);

    std::vector<LinearCounterLimit> csig0,csig1,csig2,csig3,cref0,cref1,cref2,cref3;
    std::vector<LinearCounterLimit>p_laplace;
    std::vector <NMixLogistic>lmixref,lmixsig;
    NMixLogistic ssemix;

    SSENL<15> sse[1<<12];
    SSENL<15> *psse1,*psse2;
    LinearCounterLimit *pc1,*pc2,*pc3,*pc4;
    LinearCounterLimit *pl;
    NMixLogistic *plmix;
    std::int32_t *pabuf,sample;
    std::vector <std::int32_t>msb;
    //std::int32_t n_laplace;
    //std::vector <double>weights_laplace;
    std::int32_t sigst[17];
    std::uint32_t bmask[32];
    std::int32_t maxbpn,bpn,numsamples,nrun,pestimate;
    std::uint32_t state;
    StaticLaplaceModel lm;
};

class Golomb {
  public:
    Golomb (RangeCoderSH &rc)
    :msum(0.98,1<<15),rc(rc)
    {
      lastl=0;
    }
    void Encode(std::int32_t val)
    {
      if (val<0) val=2*(-val);
      else if (val>0) val=(2*val)-1;

      std::int32_t m=(std::max)(static_cast<std::int32_t>(msum.sum),1);
      std::int32_t q=val/m;
      std::int32_t r=val-q*m;

      //for (std::int32_t i=0;i<q;i++) rc.EncodeBitOne(PSCALEh,1); // encode exponent unary
      //rc.EncodeBitOne(PSCALEh,0);

      std::int32_t ctx=1;
      for (std::int32_t i=7;i>=0;i--) {
        std::int32_t bit=(q>>i)&1;
        rc.EncodeBitOne(cnt[ctx].p1,bit);
        cnt[ctx].update(bit,250);

        ctx+=ctx+bit;
      }

      /*std::int32_t ctx=0;
      for (std::int32_t i=0;i<q;i++) {
        std::int32_t pctx=lastl+(ctx<<1);
        rc.EncodeBitOne(cnt[pctx].p1,1);
        cnt[pctx].update(1,128);
        ctx++;
        if (ctx>1) ctx=1;
      }
      std::int32_t pctx=lastl+(ctx<<1);
      rc.EncodeBitOne(cnt[pctx].p1,0);
      cnt[pctx].update(0,128);

      if (q>0) lastl=1;
      else lastl=0;*/

      if (m>1)
      {
        std::int32_t b=std::ceil(std::log(m)/std::log(2));
        std::int32_t t=(1<<b)-m;
        if (r < t) {
          for (std::int32_t i=b-2;i>=0;i--) rc.EncodeBitOne(PSCALEh,((r>>i)&1));
        } else {
          for (std::int32_t i=b-1;i>=0;i--) rc.EncodeBitOne(PSCALEh,(((r+t)>>i)&1));
        }
      }

      msum.Update(val);
    }
    std::int32_t Decode() {
      std::int32_t q=0;
      while (rc.DecodeBitOne(PSCALEh)!=0) q++;

      std::int32_t m=(std::max)(static_cast<std::int32_t>(msum.sum),1);
      std::int32_t r=0;

      if (m>1)
      {
        std::int32_t b=std::ceil(std::log(m)/std::log(2));
        std::int32_t t=(1<<b)-m;
        for (std::int32_t i=b-2;i>=0;i--) r=(r<<1)+rc.DecodeBitOne(PSCALEh);
        if (r>=t) r=((r<<1)+rc.DecodeBitOne(PSCALEh))-t;
      }

      std::int32_t val=m*q+r;
      msum.Update(val);

      if (val) {
        if (val&1) val=((val+1)>>1);
        else val=-(val>>1);
      }
      return val;
    }
    RunExp msum;
  private:
    RangeCoderSH &rc;
    LinearCounter16 cnt[512];
    std::int32_t lastl;
};

class GolombRC {
  public:
    GolombRC (RangeCoder &rc)
    :msum(0.8,1<<15),rc(rc)
    {
    }
    void Encode(std::int32_t val)
    {
      if (val<0) val=2*(-val);
      else if (val>0) val=(2*val)-1;

      std::int32_t m=(std::max)(static_cast<std::int32_t>(msum.sum),1);
      std::int32_t q=val/m;
      std::int32_t r=val-q*m;

      for (std::int32_t i=0;i<q;i++) rc.EncodeBitOne(PSCALEh,1); // encode exponent unary
      rc.EncodeBitOne(PSCALEh,0);

      rc.EncodeSymbol(r,1,m);

      msum.Update(val);
    }
    std::int32_t Decode() {
      std::int32_t q=0;
      while (rc.DecodeBitOne(PSCALEh)!=0) q++;

      std::int32_t m=(std::max)(static_cast<std::int32_t>(msum.sum),1);

      std::int32_t r=rc.DecProb(m);
      rc.DecodeSymbol(r,1);

      std::int32_t val=m*q+r;
      msum.Update(val);

      if (val) {
        if (val&1) val=((val+1)>>1);
        else val=-(val>>1);
      }
      return val;
    }
    RunExp msum;
  private:
    RangeCoder &rc;
};
