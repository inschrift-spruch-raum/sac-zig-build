#include "vle.h"

#include "../common/math.h"

BitplaneCoder::BitplaneCoder(std::int32_t maxbpn,std::int32_t numsamples)
:csig0(1<<20),csig1(1<<20),csig2(1<<20),csig3(1<<20),
cref0(1<<20),cref1(1<<20),cref2(1<<20),cref3(1<<20),
p_laplace(32),
lmixref(256,NMixLogistic(5)),lmixsig(256,NMixLogistic(3)),
ssemix(2),
msb(numsamples),
maxbpn(maxbpn),numsamples(numsamples),lm(maxbpn)
//n_laplace(32),weights_laplace(2*n_laplace+1),
{
  state=0;
  bpn=0;
  nrun=0;
  double theta=0.99;
  for (std::int32_t i=0;i<32;i++) {
    std::int32_t p = std::min(
      std::max(
        (std::int32_t)std::round((1.0 - 1.0 / (1 + std::pow(theta, 1 << i))) * PSCALE), 1
      ),
      PSCALEm
    );
    //std::cout << p << ' ';
    p_laplace[i].p1=p;
  }
  pestimate=0;
  for (std::int32_t i=0;i<32;i++) {
    bmask[i]=~((1U<<i)-1);
  }
  /*double s=35;
  for (std::int32_t i=0;i<2*n_laplace+1;i++) {
    std::int32_t idx=i-n_laplace;
    weights_laplace[i]=1.0; //exp(-(idx*idx)/(s*s));
  }*/
}

void BitplaneCoder::GetSigState(std::int32_t i)
{
  sigst[0]=msb[i];
  sigst[1]=i>0?msb[i-1]:0;
  sigst[2]=i<numsamples-1?msb[i+1]:0;
  sigst[3]=i>1?msb[i-2]:0;
  sigst[4]=i<numsamples-2?msb[i+2]:0;
  sigst[5]=i>2?msb[i-3]:0;
  sigst[6]=i<numsamples-3?msb[i+3]:0;
  sigst[7]=i>3?msb[i-4]:0;
  sigst[8]=i<numsamples-4?msb[i+4]:0;
  sigst[9]=i>4?msb[i-5]:0;
  sigst[10]=i<numsamples-5?msb[i+5]:0;
  sigst[11]=i>5?msb[i-6]:0;
  sigst[12]=i<numsamples-6?msb[i+6]:0;
  sigst[13]=i>6?msb[i-7]:0;
  sigst[14]=i<numsamples-7?msb[i+7]:0;
  sigst[15]=i>7?msb[i-8]:0;
  sigst[16]=i<numsamples-8?msb[i+8]:0;
}

std::uint32_t BitplaneCoder::GetAvgSum(std::int32_t n)
{
  std::uint64_t nsum = 0;
  std::int32_t nidx = 0;
  
  const std::int32_t start = std::max(sample - n, 0);
  const std::int32_t end = std::min(sample + n, numsamples - 1);
  
  for (std::int32_t k = start; k < sample; k++) {
    nsum += pabuf[k] & bmask[bpn];
    nidx++;
  }
  
  for (std::int32_t k = sample; k <= end; k++) {
    nsum += pabuf[k] & bmask[bpn+1];
    nidx++;
  }
  
  return nidx > 0 ? (nsum + nidx - 1) / nidx : 0;
}

std::int32_t BitplaneCoder::PredictLaplace(std::uint32_t avg_sum)
{
  double p_l=0.0;
  if (avg_sum>0) {
    double theta=std::exp(-1.0/avg_sum);
    p_l=1.0-1.0/(1+std::pow(theta,1<<bpn));
  };
  std::int32_t p1=std::min(std::max((std::int32_t)std::round(p_l*PSCALE),1),PSCALEm);
  return p1;
}

std::int32_t BitplaneCoder::PredictRef()
{
  std::int32_t val=pabuf[sample];

  std::int32_t lval=sample>0?pabuf[sample-1]:0;
  std::int32_t lval2=sample>1?pabuf[sample-2]:0;
  std::int32_t nval=sample<(numsamples-1)?pabuf[sample+1]:0;
  std::int32_t nval2=sample<(numsamples-2)?pabuf[sample+2]:0;

  std::int32_t b0=(val>>(bpn+1));
  std::int32_t b1=(lval>>(bpn));
  std::int32_t b2=(nval>>(bpn+1));
  std::int32_t b3=(lval2>>(bpn));
  std::int32_t b4=(nval2>>(bpn+1));


  std::int32_t c0=(b0<<1)<b1?1:0;
  std::int32_t c1=(b0)<b2?1:0;
  std::int32_t c2=(b0<<1)<b3?1:0;
  std::int32_t c3=(b0)<(b4)?1:0;


  std::int32_t x0=(val>>(bpn+1))<<1;
  std::int32_t x1=(lval>>bpn);
  std::int32_t x2=(nval>>(bpn+1))<<1;
  std::int32_t x3=(lval2>>(bpn));
  std::int32_t x4=(nval2>>(bpn+1))<<1;
  std::int32_t xm=(x0+x1+x2+x3+x4)/5;

  std::int32_t d0=x0>xm;
  std::int32_t d1=x1>xm;
  //std::int32_t d2=x2>xm;

  std::int32_t ctx1=(b0&15)+((b1&15)<<4)+((b2&15)<<8);
  std::int32_t ctx2=(c0+(c1<<1)+(c2<<2)+(c3<<3))+(d0<<4)+(d1<<5);
  std::int32_t ctx3=(sigst[1]+sigst[2]+sigst[3]+sigst[4]+sigst[5]+sigst[6]+sigst[7]+sigst[8]);

  pl=&p_laplace[bpn];
  pc1=&cref0[msb[sample]];
  pc2=&cref1[ctx1&255];
  pc3=&cref2[ctx2];
  pc4=&cref3[ctx3];

  std::int32_t pctx=((((pestimate>>12)<<1)+d0)<<1)+(b0&1);
  plmix=&lmixref[pctx];

  std::int32_t px=plmix->Predict({pestimate,pl->p1,pc1->p1,pc2->p1,pc3->p1});

  return px;
}

void BitplaneCoder::UpdateRef(std::int32_t bit)
{
  pl->update(bit,cnt_upd_rate_p);
  pc1->update(bit,cnt_upd_rate_ref);
  pc2->update(bit,cnt_upd_rate_ref);
  pc3->update(bit,cnt_upd_rate_ref);
  pc4->update(bit,cnt_upd_rate_ref);
  plmix->Update(bit,mix_upd_rate_ref);
  state=(state<<1)+0;
}

// count number of significant samples in neighborhood
void BitplaneCoder::CountSig(std::int32_t n,std::int32_t &n1,std::int32_t &n2)
{
  n1=n2=0;
  for (std::int32_t i=1;i<=n;i++) {
    if (sample-i>=0) {
       if (msb[sample-i]) n1+=1;
       if (msb[sample-i]>bpn) n2+=1;
    }
    if (sample+i<numsamples-1) {
       if (msb[sample+i]) n1+=1;
       if (msb[sample+i]>bpn) n2+=1;
    }
  }
}

std::int32_t BitplaneCoder::PredictSig()
{
  std::int32_t ctx1=0;
  for (std::int32_t i=0;i<16;i++)
    if (sigst[i+1]) ctx1+=1<<i;

  std::int32_t n1,n2;
  CountSig(32,n1,n2);
  std::int32_t ctx2=n2;

  pl=&p_laplace[bpn];
  pc1=&csig0[ctx1];
  pc2=&csig1[ctx2];

  std::int32_t mixctx=((state&15)<<3)+((n1>=3?3:n1)<<1)+(n2>0?1:0);
  plmix=&lmixsig[mixctx];
  std::int32_t p_mix=plmix->Predict({pl->p1,pc1->p1,pc2->p1});
  return p_mix;
}

void BitplaneCoder::UpdateSig(std::int32_t bit)
{
  pl->update(bit,cnt_upd_rate_p);
  pc1->update(bit,cnt_upd_rate_sig);
  pc2->update(bit,cnt_upd_rate_sig);
  plmix->Update(bit,mix_upd_rate_sig);
  state=(state<<1)+1;
}

std::int32_t BitplaneCoder::PredictSSE(std::int32_t p1)
{
  std::int32_t ctx1=((pestimate>>11)<<1)+(sigst[0]?1:0);
  std::int32_t ctx2=32+(sigst[0]?1:0)+((sigst[1]?1:0)<<1)+((sigst[2]?1:0)<<2)+((sigst[3]?1:0)<<3)+((sigst[4]?1:0)<<4)+((sigst[5]?1:0)<<5)+((sigst[6]?1:0)<<6);
  psse1=&sse[ctx1];
  psse2=&sse[ctx2];
  std::int32_t pr1=psse1->Predict(p1);
  std::int32_t pr2=psse2->Predict(pr1);
  return ssemix.Predict({(pr1+pr2+1)>>1,p1});
}

void BitplaneCoder::UpdateSSE(std::int32_t bit)
{
  psse1->Update(bit,cntsse_upd_rate);
  psse2->Update(bit,cntsse_upd_rate);
  ssemix.Update(bit,mixsse_upd_rate);
}

void BitplaneCoder::Encode(EncodeP1 encode_p1,std::int32_t *abuf)
{
  pabuf=abuf;
  for (bpn=maxbpn;bpn>=0;bpn--)  {
    state=0;
    for (sample=0;sample<numsamples;sample++) {
      std::uint32_t avg_sum = GetAvgSum(32);
      pestimate=PredictLaplace(avg_sum);//lm.Predict(avg_sum,bpn);
      GetSigState(sample);
      std::int32_t bit=(pabuf[sample]>>bpn)&1;
      std::int32_t p=0;
      if (sigst[0]) { // coef is significant, refine
        p=PredictSSE(PredictRef());
        encode_p1(p,bit);
        UpdateRef(bit);
        UpdateSSE(bit);
      } else { // coef is insignificant
        p=PredictSSE(PredictSig());
        encode_p1(p,bit);
        UpdateSig(bit);
        UpdateSSE(bit);
        if (bit) msb[sample]=bpn;
      }
    }
  }
}

void BitplaneCoder::Decode(DecodeP1 decode_p1,std::int32_t *buf)
{
  std::int32_t bit;
  pabuf=buf;
  for (std::int32_t i=0;i<numsamples;i++) buf[i]=0;
  for (bpn=maxbpn;bpn>=0;bpn--)  {
    state=0;
    for (sample=0;sample<numsamples;sample++) {
      std::uint32_t avg_sum=GetAvgSum(32);
      pestimate=PredictLaplace(avg_sum);//lm.Predict(avg_sum,bpn);
      GetSigState(sample);
      if (sigst[0]) { // coef is significant, refine
        bit=decode_p1(PredictSSE(PredictRef()));
        UpdateRef(bit);
        UpdateSSE(bit);
        if (bit) buf[sample]+=(1<<bpn);
       } else { // coef is insignificant
         bit=decode_p1(PredictSSE(PredictSig()));
         UpdateSig(bit);
         UpdateSSE(bit);
         if (bit) {
           buf[sample]+=(1<<bpn);
           msb[sample]=bpn;
          }
        }
    }
  }
  for (std::int32_t i=0;i<numsamples;i++) buf[i]=MathUtils::U2S(buf[i]);
}
