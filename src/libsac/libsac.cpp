#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <numeric>
#include <print>
#include <thread>
#include <vector>

#include "../common/timer.h"
#include "../opt/cma.h"
#include "../opt/dds.h"
#include "../opt/de.h"
#include "libsac.h"
#include "pred.h"
#include "sparse.h"

FrameCoder::FrameCoder(std::int32_t numchannels,std::int32_t framesize,const tsac_cfg &cfg)
:numchannels_(numchannels),framesize_(framesize),cfg(cfg)
{
  profile_size_bytes_=base_profile.LoadBaseProfile()*4;

  framestats.resize(numchannels);
  samples.resize(numchannels);
  error.resize(numchannels);
  s2u_error.resize(numchannels);
  s2u_error_map.resize(numchannels);
  pred.resize(numchannels);
  for (std::int32_t i=0;i<numchannels;i++) {
    samples[i].resize(framesize);
    error[i].resize(framesize);
    pred[i].resize(framesize);
    s2u_error[i].resize(framesize);
    s2u_error_map[i].resize(framesize);
  }
  encoded.resize(numchannels);
  enc_temp1.resize(numchannels);
  enc_temp2.resize(numchannels);
  numsamples_=0;
}

void FrameCoder::SetParam(Predictor::tparam &param,const SacProfile &profile,bool optimize) const
{
  if (optimize) { param.k=cfg.ocfg.optk;
  } else { param.k=1;
}

  param.lambda0=param.lambda1=profile.Get(0);
  param.ols_nu0=param.ols_nu1=profile.Get(1);

  param.vn0={static_cast<std::int32_t>(std::round(profile.Get(28))),static_cast<std::int32_t>(std::round(profile.Get(29))),static_cast<std::int32_t>(std::round(profile.Get(30))),static_cast<std::int32_t>(std::round(profile.Get(37)))};
  param.vn1={static_cast<std::int32_t>(std::round(profile.Get(31))),static_cast<std::int32_t>(std::round(profile.Get(32))),static_cast<std::int32_t>(std::round(profile.Get(33))),static_cast<std::int32_t>(std::round(profile.Get(38)))};

  param.vmu0={profile.Get(2)/static_cast<double>(param.vn0[0]),profile.Get(3)/static_cast<double>(param.vn0[1]),profile.Get(4)/static_cast<double>(param.vn0[2]),profile.Get(5)/static_cast<double>(param.vn0[3])};
  param.vmudecay0={profile.Get(6),profile.Get(39),profile.Get(46),profile.Get(47)};
  param.vpowdecay0={profile.Get(7),profile.Get(8),profile.Get(50),profile.Get(51)};
  param.mu_mix0=profile.Get(10);
  param.mu_mix_beta0=profile.Get(11);

  param.lambda1=profile.Get(12);
  param.ols_nu1=profile.Get(13);
  param.vmu1={profile.Get(14)/static_cast<double>(param.vn1[0]),profile.Get(15)/static_cast<double>(param.vn1[1]),profile.Get(16)/static_cast<double>(param.vn1[2]),profile.Get(17)/static_cast<double>(param.vn1[3])};
  param.vmudecay1={profile.Get(18),profile.Get(40),profile.Get(48),profile.Get(49)};
  param.vpowdecay1={profile.Get(19),profile.Get(20),profile.Get(21),profile.Get(52)};
  param.mu_mix1=profile.Get(22);
  param.mu_mix_beta1=profile.Get(23);

  param.nA= static_cast<std::int32_t>(std::round(profile.Get(24)));
  param.nB= static_cast<std::int32_t>(std::round(profile.Get(25)));
  param.nS0=static_cast<std::int32_t>(std::round(profile.Get(26)));
  param.nS1=static_cast<std::int32_t>(std::round(profile.Get(27)));
  param.nM0=static_cast<std::int32_t>(std::round(profile.Get(9)));

  param.beta_sum0=profile.Get(34);
  param.beta_pow0=profile.Get(35);
  param.beta_add0=profile.Get(36);

  param.beta_sum1=profile.Get(34);
  param.beta_pow1=profile.Get(35);
  param.beta_add1=profile.Get(36);

  param.lm_n=static_cast<int32_t>(std::round(profile.Get(41)));
  param.lm_alpha=profile.Get(42);

  param.bias_mu0=profile.Get(43);
  param.bias_mu1=profile.Get(44);

  param.bias_scale0=param.bias_scale1=static_cast<int32_t>(std::round(profile.Get(45)));

  param.ch_ref=0;
  if (param.nS1 < 0) {
    param.nS1 = -param.nS1;
    param.ch_ref=1;
  } //else if (param.nS1==0) param.nS1=1;
}

void FrameCoder::PredictFrame(const SacProfile &profile,tch_samples &error,std::int32_t from,std::int32_t numsamples,bool optimize)
{

  Predictor::tparam param;
  SetParam(param,profile,optimize);
  Predictor pr(param);

  auto eprocess=[&](std::int32_t ch_p,std::int32_t ch,std::int32_t val,std::int32_t idx) {
      double pd=pr.predict(ch_p);
      std::int32_t pi=std::clamp(static_cast<std::int32_t>(std::round(pd)),framestats[ch].minval,framestats[ch].maxval);
      if (!optimize) { pred[ch][idx]=pi+framestats[ch].mean;
}
      error[ch][idx]=val-pi; // needed for cost-function within optimize
      pr.update(ch_p,val);
  };


  if (numchannels_==1) {
    const auto &src=samples[0];
    for (std::int32_t idx=0;idx<numsamples;idx++)
    {
      pr.fillbuf_ch0(&src[from],idx,&src[from],idx);
      eprocess(0,0,src[from + idx],idx);
    }
  } else if (numchannels_==2) {
    std::int32_t ch0=param.ch_ref;
    std::int32_t ch1=1-ch0;

    const auto &src0=samples[ch0];
    const auto &src1=samples[ch1];

    std::int32_t idx0=0;
    std::int32_t idx1=0;
    while (idx0<numsamples || idx1<numsamples)
    {
      if (idx0<numsamples) {
        pr.fillbuf_ch0(&src0[from],idx0,&src1[from],idx1);
        eprocess(0,ch0,src0[from + idx0],idx0);
        idx0++;
      }
      if (idx0>=param.nS1) {
        pr.fillbuf_ch1(&src0[from],&src1[from],idx1,numsamples);
        eprocess(1,ch1,src1[from + idx1],idx1);
        idx1++;
      }
    }
  }
}

void FrameCoder::UnpredictFrame(const SacProfile &profile,std::int32_t numsamples)
{
  Predictor::tparam param;
  SetParam(param,profile,false);
  Predictor pr(param);

  auto dprocess=[&](std::int32_t ch_p,std::int32_t ch,std::vector<std::int32_t> &dst,std::int32_t idx) {
    const double pd=pr.predict(ch_p);
    const std::int32_t pi=std::clamp(static_cast<std::int32_t>(std::round(pd)),framestats[ch].minval,framestats[ch].maxval);


    if (framestats[ch].enc_mapped) {
      dst[idx]=pi+framestats[ch].mymap.Unmap(pi+framestats[ch].mean,error[ch][idx]);
    } else {
      dst[idx]=pi+error[ch][idx];
}

    pr.update(ch_p,dst[idx]);
  };

  if (numchannels_==1) {
    auto &dst=samples[0];
    for (std::int32_t idx=0;idx<numsamples;idx++)
    {
      pr.fillbuf_ch0(dst.data(),idx,dst.data(),idx);
      dprocess(0,0,dst,idx);
    }
  } else if (numchannels_==2) {
    std::int32_t ch0=param.ch_ref;
    std::int32_t ch1=1-ch0;

    auto &dst0=samples[ch0];
    auto &dst1=samples[ch1];
    std::int32_t idx0=0;
    std::int32_t idx1=0;
    while (idx0<numsamples || idx1<numsamples)
    {
      if (idx0<numsamples) {
        pr.fillbuf_ch0(dst0.data(),idx0,dst1.data(),idx1);
        dprocess(0,ch0,dst0,idx0);
        idx0++;
      }
      if (idx0>=param.nS1) {
        pr.fillbuf_ch1(dst0.data(),dst1.data(),idx1,numsamples);
        dprocess(1,ch1,dst1,idx1);
        idx1++;
      }
    }
  }

  // add mean
  for (std::int32_t ch=0;ch<numchannels_;ch++) {
    if (framestats[ch].mean!=0) {
      for (std::int32_t i=0;i<numsamples;i++) { samples[ch][i]+=framestats[ch].mean;
}
}
  }
}

std::size_t FrameCoder::EncodeMonoFrame_Normal(std::int32_t ch,std::int32_t numsamples,BufIO &buf)
{
  buf.Reset();
  RangeCoderSH rc(buf);
  rc.Init();

  BitplaneCoder bc(framestats[ch].maxbpn,numsamples);
  std::int32_t *psrc=s2u_error[ch].data();
  bc.Encode(rc.encode_p1,psrc);
  rc.Stop();
  return buf.GetBufPos();
}

std::size_t FrameCoder::EncodeMonoFrame_Mapped(std::int32_t ch,std::int32_t numsamples,BufIO &buf)
{
  buf.Reset();

  RangeCoderSH rc(buf);
  rc.Init();

  BitplaneCoder bc(framestats[ch].maxbpn_map,numsamples);

  MapEncoder me(rc,framestats[ch].mymap.usedl,framestats[ch].mymap.usedh);
  me.Encode();
  bc.Encode(rc.encode_p1,s2u_error_map[ch].data());
  rc.Stop();
  return buf.GetBufPos();
}

double FrameCoder::CalcRemapError(std::int32_t ch, std::int32_t numsamples) {
    std::int32_t emax_map = 1;
    std::int64_t sum_error = 0;
    std::int64_t sum_emap = 0;

    for (std::int32_t i = 0; i < numsamples; i++) {
        std::int32_t map_e = framestats[ch].mymap.Map(pred[ch][i], error[ch][i]);
        std::int32_t map_ue = MathUtils::S2U(map_e);
        sum_emap += std::abs(map_e);
        s2u_error_map[ch][i] = map_ue;
        
        emax_map = std::max(emax_map, map_ue);
        
        sum_error += std::abs(error[ch][i]);
    }
    
    framestats[ch].maxbpn_map = std::ilogb(emax_map);

    double ent1 = 0.0;
    double ent2 = 0.0;
    if (numsamples > 0) {
      ent1 = static_cast<double>(sum_error) / static_cast<double>(numsamples);
      ent2 = static_cast<double>(sum_emap) / static_cast<double>(numsamples);
    }
    
    double r = 1.0;
    if (ent2 != 0.0) { r = ent1 / ent2;
}
    
    if (cfg.verbose_level > 0){
      std::println("  cost pcm-model: {:.5f} {:.5f} {:.5f}", ent1, ent2, r);
    }
    
    return r;
}

void FrameCoder::EncodeMonoFrame(std::int32_t ch,std::int32_t numsamples)
{
  if (cfg.sparse_pcm==0) {
    EncodeMonoFrame_Normal(ch,numsamples,enc_temp1[ch]);
    framestats[ch].enc_mapped=false;
    encoded[ch]=enc_temp1[ch];
  } else {
    double r = CalcRemapError(ch,numsamples);
    auto size_normal=static_cast<std::int32_t>(EncodeMonoFrame_Normal(ch,numsamples,enc_temp1[ch]));
    framestats[ch].enc_mapped=false;
    encoded[ch]=enc_temp1[ch];

    if (r > 1.05)
    {
      auto size_mapped=static_cast<std::int32_t>(EncodeMonoFrame_Mapped(ch,numsamples,enc_temp2[ch]));
      if (size_mapped<size_normal)
      {
        if (cfg.verbose_level>0) {
          std::cout << "  sparse frame " << size_normal << " -> " << size_mapped << " (" << (size_mapped-size_normal) << ")\n";
        }
        framestats[ch].enc_mapped=true;
        encoded[ch]=enc_temp2[ch];
      }
    }
  }
}

void FrameCoder::DecodeMonoFrame(std::int32_t ch,std::int32_t numsamples)
{
  std::int32_t *dst=error[ch].data();
  BufIO &buf=encoded[ch];
  buf.Reset();

  RangeCoderSH rc(buf,1);
  rc.Init();
  if (framestats[ch].enc_mapped) {
    framestats[ch].mymap.Reset();
    MapEncoder me(rc,framestats[ch].mymap.usedl,framestats[ch].mymap.usedh);
    me.Decode();
    //std::cout << buf.GetBufPos() << std::endl;
  }

  BitplaneCoder bc(framestats[ch].maxbpn,numsamples);
  bc.Decode(rc.decode_p1,dst);
  rc.Stop();
}


void FrameCoder::PrintProfile(SacProfile &profile)
{
    Predictor::tparam param;
    SetParam(param,profile);

    std::cout << '\n';
    std::cout << "lpc (nA " << std::round(profile.Get(24)) << " nM0 " << std::round(profile.Get(9));
    std::cout << ") (nB " << std::round(profile.Get(25)) << " nS0 " << std::round(profile.Get(26)) << " nS1 " << std::round(profile.Get(27)) << ")\n";
    std::cout << "lpc nu " << param.ols_nu0 << ' ' << param.ols_nu1 << '\n';
    std::cout << "lpc cov0 " << param.beta_sum0 << ' ' << param.beta_pow0 << ' ' << param.beta_add0 << "\n";
    std::cout << "lms0 ";
    for (std::int32_t i=28;i<=30;i++) { std::cout << std::round(profile.Get(i)) << ' ';
}
    std::cout << std::round(profile.Get(37));
    std::cout << '\n';
    std::cout << "lms1 ";
    for (std::int32_t i=31;i<=33;i++) { std::cout << std::round(profile.Get(i)) << ' ';
}
    std::cout << std::round(profile.Get(38));
    std::cout << '\n';
    std::cout << "mu ";
    for (std::size_t i=0;i<std::size(param.vmu0);i++) {
      std::cout << (param.vmu0[i]*param.vn0[i]) << ' ';
}
    std::cout << '\n';
    std::cout << "mu_decay ";
    for (const auto &x : param.vmudecay0) {
      std::cout << x << ' ';
}
    std::cout << '\n';
    std::cout << "pow_decay ";
    for (const auto &x : param.vpowdecay0) {
      std::cout << x << ' ';
}
    std::cout << '\n';

    std::cout << "mu mix mu " << param.mu_mix0 << " " << param.mu_mix1 << '\n';
    std::cout << "mu mix beta " << param.mu_mix_beta0 << " " << param.mu_mix_beta1 << '\n';
    std::cout << "ch-ref " << param.ch_ref << "\n";
    std::cout << "bias mu " << param.bias_mu0 << ", " << param.bias_mu1 << " scale " << (1U<<static_cast<std::uint32_t>(param.bias_scale0)) << ' ' << (1U<<static_cast<std::uint32_t>(param.bias_scale1)) << '\n';
    std::cout << "lm " << param.lm_n << " gamma " << param.lm_alpha << '\n';
}

double FrameCoder::GetCost(const std::shared_ptr<CostFunction> &func,const tch_samples &samples,std::size_t samples_to_optimize) const
{
  // return a span over samples
  const auto span_ch = [=](std::int32_t ch){
    return std::span{samples[ch].data(),samples_to_optimize};
  };

  double cost=0.0;
  if (cfg.mt_mode>1 && numchannels_>1) {

    std::vector <std::future<double>> threads;
    threads.reserve(numchannels_);
for (std::int32_t ch=0;ch<numchannels_;ch++) {
        threads.emplace_back(std::async([=]{return func->Calc(span_ch(ch));}));
}

    for (auto &thread : threads) {
      cost += thread.get();
}

  } else {
    for (std::int32_t ch=0;ch<numchannels_;ch++) {
      cost += func->Calc(span_ch(ch));
}
  }
  return cost;
}

void FrameCoder::Optimize(const FrameCoder::toptim_cfg &ocfg,SacProfile &profile,const std::vector<std::int32_t>&params_to_optimize)
{
  std::int32_t samples_to_optimize=std::min(numsamples_,static_cast<std::int32_t>(std::ceil(framesize_*ocfg.fraction)));
  const std::int32_t start_pos=(numsamples_-samples_to_optimize)/2;

  std::shared_ptr<CostFunction> CostFunc;
  switch (ocfg.optimize_cost)  {
    case FrameCoder::SearchCost::L1:      CostFunc = std::make_shared<CostL1>();break;
    case FrameCoder::SearchCost::RMS:     CostFunc = std::make_shared<CostRMS>();break;
    case FrameCoder::SearchCost::Golomb:  CostFunc = std::make_shared<CostGolomb>();break;
    case FrameCoder::SearchCost::Entropy: CostFunc = std::make_shared<CostEntropy>();break;
    case FrameCoder::SearchCost::Bitplane:CostFunc = std::make_shared<CostBitplane>();break;
    default:std::cerr << "  error: unknown FramerCoder::CostFunction\n";return;
  }

  const std::size_t ndim=params_to_optimize.size();
  vec1D xstart(ndim); // starting vector
  Opt::box_const pb(ndim); // set constraints
  for (std::size_t i=0;i<ndim;i++) {
    pb[i].xmin=profile.coefs[params_to_optimize[i]].vmin;
    pb[i].xmax=profile.coefs[params_to_optimize[i]].vmax;
    xstart[i]=profile.coefs[params_to_optimize[i]].vdef;
  }

  auto cost_func=[&](const vec1D &x) {
    // create thread safe copies for error and profile
    tch_samples tmp_error(numchannels_,std::vector<std::int32_t>(samples_to_optimize));
    SacProfile tmp_profile=profile;

    for (std::size_t i=0;i<ndim;i++) { tmp_profile.coefs[params_to_optimize[i]].vdef=static_cast<float>(x[i]);
}

    PredictFrame(tmp_profile,tmp_error,start_pos,samples_to_optimize,true);
    return GetCost(CostFunc,tmp_error,samples_to_optimize);
  };

  if (cfg.verbose_level>0) {
    std::string opt_str;
    if (ocfg.optimize_search==FrameCoder::SearchMethod::DDS) { opt_str="DDS";
}
    if (ocfg.optimize_search==FrameCoder::SearchMethod::DE) { opt_str="DE";
    } else if (ocfg.optimize_search==FrameCoder::SearchMethod::CMA) { opt_str="CMA";
}
    std::cout << "\n " << opt_str << " " << ocfg.maxnfunc << "= ";
  }

  std::unique_ptr<Opt> myOpt;

  if (ocfg.optimize_search==FrameCoder::SearchMethod::DDS) {
    myOpt = std::make_unique<OptDDS>(ocfg.dds_cfg,pb,cfg.verbose_level);
  } else if (ocfg.optimize_search==FrameCoder::SearchMethod::DE) {
    myOpt = std::make_unique<OptDE>(ocfg.de_cfg,pb,cfg.verbose_level);
  } else if (ocfg.optimize_search==FrameCoder::SearchMethod::CMA) {
    myOpt = std::make_unique<OptCMA>(ocfg.cma_cfg,pb,cfg.verbose_level);
}

  Opt::ppoint ret = myOpt->run(cost_func,xstart);

  // save optimal vector to baseprofile
  for (std::size_t i=0;i<ndim;i++) {
    profile.coefs[params_to_optimize[i]].vdef=static_cast<float>(ret.second[i]);
}

  if (cfg.verbose_level>0) {
    PrintProfile(profile);
  }
}

void FrameCoder::CnvError_S2U(const tch_samples &error,std::int32_t numsamples)
{
  for (std::int32_t ch=0;ch<numchannels_;ch++)
  {
    std::int32_t emax = 1;
    for (std::int32_t i=0;i<numsamples;i++) {
      const std::int32_t e_s2u=MathUtils::S2U(error[ch][i]);
      emax = std::max(e_s2u, emax);
      s2u_error[ch][i]=e_s2u;
    }
    framestats[ch].maxbpn=std::ilogb(emax);
  }
}

void FrameCoder::Predict()
{
  for (std::int32_t ch=0;ch<numchannels_;ch++)
  {
    AnalyseMonoChannel(ch,numsamples_);
    if (cfg.sparse_pcm != 0) {
      framestats[ch].mymap.Reset();
      framestats[ch].mymap.Analyse(samples[ch], numsamples_);
    }
    if (cfg.zero_mean==0) {
      framestats[ch].mean = 0;
    } else if (framestats[ch].mean!=0) {
      for (std::int32_t i=0;i<numsamples_;i++) { samples[ch][i] -= framestats[ch].mean;
}
      framestats[ch].minval -= framestats[ch].mean;
      framestats[ch].maxval -= framestats[ch].mean;
    }
  }

  if (cfg.optimize != 0)
  {
    // reset profile params
    // otherwise: starting point for optimization is the best point from the last frame
    if (cfg.ocfg.reset != 0) {
      base_profile.LoadBaseProfile();
}

    // optimize all params
    std::vector<std::int32_t>lparam_base(base_profile.coefs.size());
    std::iota(std::begin(lparam_base),std::end(lparam_base),0);

    Optimize(cfg.ocfg,base_profile,lparam_base);
  }
  PredictFrame(base_profile,error,0,numsamples_,false);
  CnvError_S2U(error,numsamples_);
}

void FrameCoder::Unpredict()
{
  UnpredictFrame(base_profile,numsamples_);
}

void FrameCoder::Encode()
{
  if ((cfg.mt_mode != 0) && numchannels_>1)  {
    std::vector <std::jthread> threads;
    threads.reserve(numchannels_);
for (std::int32_t ch=0;ch<numchannels_;ch++) {
      threads.emplace_back(std::jthread(&FrameCoder::EncodeMonoFrame,this,ch,numsamples_));
}
  } else {
    for (std::int32_t ch=0;ch<numchannels_;ch++) { EncodeMonoFrame(ch,numsamples_);
}
}
}

void FrameCoder::Decode()
{
  if ((cfg.mt_mode != 0) && numchannels_>1) {
    std::vector <std::jthread> threads;
    threads.reserve(numchannels_);
for (std::int32_t ch=0;ch<numchannels_;ch++) {
      threads.emplace_back(std::jthread(&FrameCoder::DecodeMonoFrame,this,ch,numsamples_));
}
  } else {
    for (std::int32_t ch=0;ch<numchannels_;ch++) {
      DecodeMonoFrame(ch,numsamples_);
}
}
}

void FrameCoder::EncodeProfile(const SacProfile &profile,std::vector <std::uint8_t>&buf)
{
  //assert(sizeof(float)==4);
  //std::cout << "number of coefs: " << profile.coefs.size() << " (" << profile_size_bytes_ << ")" << std::endl;

  std::uint32_t ix = 0;
  for (std::size_t i=0;i<profile.coefs.size();i++) {
     memcpy(&ix,&profile.coefs[i].vdef,4);
     //ix=*((std::uint32_t*)&profile.coefs[i].vdef);
     BitUtils::put32LH(std::span<std::uint8_t, 4>(&buf[4*i], 4),ix);
  }
}

void FrameCoder::DecodeProfile(SacProfile &profile,const std::vector <std::uint8_t>&buf)
{
  std::uint32_t ix = 0;
  for (std::size_t i=0;i<profile.coefs.size();i++) {
     ix=BitUtils::get32LH(std::span<const std::uint8_t, 4>(&buf[4*i], 4));
     memcpy(&profile.coefs[i].vdef,&ix,4);
     //profile.coefs[i].vdef=*((float*)&ix);
  }
}

std::int32_t FrameCoder::WriteBlockHeader(std::fstream &file, const std::vector<SacProfile::FrameStats> &framestats,std::int32_t ch)
{
  std::array<std::uint8_t, 32> buf{};
  BitUtils::put32LH(std::span<std::uint8_t, 4>(buf.data(), 4),framestats[ch].blocksize);
  BitUtils::put32LH(std::span<std::uint8_t, 4>(&buf[4], 4),static_cast<std::uint32_t>(framestats[ch].mean));
  BitUtils::put32LH(std::span<std::uint8_t, 4>(&buf[8], 4),static_cast<std::uint32_t>(framestats[ch].minval));
  BitUtils::put32LH(std::span<std::uint8_t, 4>(&buf[12], 4),static_cast<std::uint32_t>(framestats[ch].maxval));
  std::uint16_t flag=0;
  if (framestats[ch].enc_mapped) {
    flag|=(1U<<9U);
    flag|=static_cast<uint32_t>(framestats[ch].maxbpn_map);
  } else {
    flag|=static_cast<uint32_t>(framestats[ch].maxbpn);
  }
  BitUtils::put16LH(std::span<std::uint8_t, 2>(&buf[16], 2),flag);
  file.write(reinterpret_cast<char*>(buf.data()),18);
  return 18;
}

std::int32_t FrameCoder::ReadBlockHeader(std::fstream &file, std::vector<SacProfile::FrameStats> &framestats,std::int32_t ch)
{
  std::array<std::uint8_t, 32> buf{};
  file.read(reinterpret_cast<char*>(buf.data()),18);

  framestats[ch].blocksize=static_cast<std::int32_t>(BitUtils::get32LH(std::span<std::uint8_t, 4>(buf.data(), 4)));
  framestats[ch].mean=static_cast<std::int32_t>(BitUtils::get32LH(std::span<std::uint8_t, 4>(&buf[4], 4)));
  framestats[ch].minval=static_cast<std::int32_t>(BitUtils::get32LH(std::span<std::uint8_t, 4>(&buf[8], 4)));
  framestats[ch].maxval=static_cast<std::int32_t>(BitUtils::get32LH(std::span<std::uint8_t, 4>(&buf[12], 4)));
  std::uint16_t flag=BitUtils::get16LH(std::span<std::uint8_t, 2>(&buf[16], 2));
  framestats[ch].enc_mapped = (flag>>9U) != 0;
  framestats[ch].maxbpn=static_cast<std::int32_t>(flag&0xffU);
  return 18;
}

void FrameCoder::WriteEncoded(AudioFile<AudioFileBase::Mode::Write> &fout)
{
  std::array<std::uint8_t, 12> buf{};
  BitUtils::put32LH(std::span<std::uint8_t, 4>(buf.data(), 4),numsamples_);
  fout.file.write(reinterpret_cast<char*>(buf.data()),4);
  std::vector <std::uint8_t>profile_buf(profile_size_bytes_);
  EncodeProfile(base_profile,profile_buf);
  fout.file.write(reinterpret_cast<char*>(profile_buf.data()),profile_size_bytes_);
  for (std::int32_t ch=0;ch<numchannels_;ch++) {
    framestats[ch].blocksize = static_cast<int32_t>(encoded[ch].GetBufPos());
    WriteBlockHeader(fout.file, framestats, ch);
    fout.Write(encoded[ch].GetBuf(),framestats[ch].blocksize);
  }
}

void FrameCoder::ReadEncoded(AudioFile<AudioFileBase::Mode::Read> &fin)
{
  std::array<std::uint8_t, 8> buf{};
  fin.file.read(reinterpret_cast<char*>(buf.data()),4);
  numsamples_=static_cast<int32_t>(BitUtils::get32LH(std::span<std::uint8_t, 4>(buf.data(), 4)));
  std::vector <std::uint8_t>profile_buf(profile_size_bytes_);
  fin.file.read(reinterpret_cast<char*>(profile_buf.data()),profile_size_bytes_);
  DecodeProfile(base_profile,profile_buf);

  for (std::int32_t ch=0;ch<numchannels_;ch++) {
    ReadBlockHeader(fin.file, framestats, ch);
    fin.Read(encoded[ch].GetBuf(),framestats[ch].blocksize);
  }
}

double FrameCoder::AnalyseStereoChannel(std::int32_t ch0, std::int32_t ch1, std::int32_t numsamples)
{
  auto &src0=samples[ch0];
  auto &src1=samples[ch1];
  std::int64_t sum0=0;
  std::int64_t sum1=0;
  std::int64_t sum_m=0;
  std::int64_t sum_s=0;
  for (std::int32_t i=0;i<numsamples;i++) {
    sum0+=static_cast<std::int64_t>(std::fabs(src0[i]));
    sum1+=static_cast<std::int64_t>(std::fabs(src1[i]));
    std::int32_t m=(src0[i]+src1[i]) / 2;
    std::int32_t s=(src0[i]-src1[i]);

    sum_m+=static_cast<std::int64_t>(std::fabs(m));
    sum_s+=static_cast<std::int64_t>(std::fabs(s));
  }
  std::int64_t c0 = sum0+sum1;
  std::int64_t c1 = sum_m+sum_s;
  return static_cast<double>(c0) / static_cast<double>(c1);
}

void FrameCoder::ApplyMs(std::int32_t ch0, std::int32_t ch1, std::int32_t numsamples)
{
  auto &src0=samples[ch0];
  auto &src1=samples[ch1];
  for (std::int32_t i=0;i<numsamples;i++) {
    std::int32_t m=(src0[i]+src1[i]) / 2;
    std::int32_t s=(src0[i]-src1[i]);
    src0[i]=m;
    src1[i]=s;
  }
}

void FrameCoder::AnalyseMonoChannel(std::int32_t ch, std::int32_t numsamples)
{
  auto &src=samples[ch];

  if (numsamples != 0) {
    std::int64_t sum=0;
    for (std::int32_t i=0;i<numsamples;i++) {
        sum += src[i];
    }
    framestats[ch].mean = static_cast<std::int32_t>(std::floor(static_cast<double>(sum) / static_cast<double>(numsamples)));

    std::int32_t minval = std::numeric_limits<std::int32_t>::max();
    std::int32_t maxval = std::numeric_limits<std::int32_t>::min();
    for (std::int32_t i=0;i<numsamples;i++) {
      const std::int32_t val=src[i];
      maxval = std::max(val, maxval);
      minval = std::min(val, minval);
    }
    framestats[ch].minval = minval;
    framestats[ch].maxval = maxval;
    if (cfg.verbose_level>0) {
      std::cout << "  ch" << ch << " samples=" << numsamples;
      std::cout << ",mean=" << framestats[ch].mean << ",min=" << framestats[ch].minval << ",max=" << framestats[ch].maxval << "\n";
    }
  }
}

void Codec::PrintProgress(std::int32_t samplesprocessed,std::int32_t totalsamples)
{
  double r=samplesprocessed*100.0/static_cast<double>(totalsamples);
  std::cout << "  " << samplesprocessed << "/" << totalsamples << ":" << std::setw(6) << miscUtils::ConvertFixed(r,1) << "%\r";
}

void Codec::ScanFrames(Sac<AudioFileBase::Mode::Read> &mySac)
{
  std::vector<SacProfile::FrameStats> framestats(mySac.getNumChannels());
  std::streampos fsize=mySac.getFileSize();

  SacProfile profile_tmp; //create dummy profile
  profile_tmp.LoadBaseProfile();
  const std::int32_t size_profile_bytes=static_cast<std::int32_t>(profile_tmp.coefs.size())*4;

  std::int32_t frame_num=1;
  std::int32_t coef_hdr_size=0;
  std::int32_t block_hdr_size=0;
  while (mySac.file.tellg()<fsize) {
    std::array<std::uint8_t, 12> buf{};
    mySac.file.read(reinterpret_cast<char*>(buf.data()),4);
    std::int32_t numsamples=static_cast<std::int32_t>(BitUtils::get32LH(std::span<std::uint8_t, 4>(buf.data(), 4)));
    std::cout << "Frame " << frame_num << ": " << numsamples << " samples "<< '\n';

    mySac.file.seekg(size_profile_bytes,std::ios_base::cur); // skip profile coefs
    coef_hdr_size += size_profile_bytes;


    for (std::int32_t ch=0;ch<mySac.getNumChannels();ch++) {
      std::int32_t num_bytes=FrameCoder::ReadBlockHeader(mySac.file, framestats, ch);
      block_hdr_size += num_bytes;
      std::cout << "  Channel " << ch << ": " << framestats[ch].blocksize << " bytes\n";
      std::cout << "    Bpn: " << framestats[ch].maxbpn << ", sparse_pcm: " << (framestats[ch].enc_mapped) << '\n';
      std::cout << "    mean: " << framestats[ch].mean << ", min: " << framestats[ch].minval << ", max: " << framestats[ch].maxval << '\n';
      mySac.file.seekg(framestats[ch].blocksize, std::ios_base::cur);
    }
    frame_num++;
  }
  std::cout << "Frames   " << (frame_num-1) << '\n';
  std::cout << "Hdr_size " << (coef_hdr_size+block_hdr_size) << " (coefs " << coef_hdr_size << ",block " << block_hdr_size << ")\n";
}


std::pair<double,double> Codec::AnalyseSparse(std::span<const std::int32_t> buf)
{
  SparsePCM spcm;
  spcm.Analyse(buf);

  return {spcm.fraction_used,spcm.fraction_cost};
}

void Codec::PushState(
  std::vector<Codec::tsub_frame>& sub_frames, Codec::tsub_frame& curframe,
  std::int32_t min_frame_length, std::int32_t block_state = -1, std::int32_t samples_block = 0
) const {
  if(block_state == curframe.state) {
    curframe.length += samples_block;
    return;
  }
  if(curframe.length < min_frame_length && (sub_frames.size() != 0U)) { // extend
    sub_frames.back().length += curframe.length;
    return;
  }

  if(opt_.verbose_level > 1) {
    std::cout << "push subframe of length " << curframe.length << " samples\n";
}
  sub_frames.push_back(curframe);

  if(samples_block != 0) {
    curframe.state = block_state; // set new blockstate
    curframe.start += curframe.length;
    curframe.length = samples_block;
  }
}

std::vector<Codec::tsub_frame> Codec::Analyse(const std::vector <std::vector<std::int32_t>>&samples,std::int32_t blocksamples,std::int32_t min_frame_length,std::int32_t samples_read)
{
  std::vector<Codec::tsub_frame> sub_frames;

  std::int32_t samples_processed=0;
  std::int32_t nblock=0;

  Codec::tsub_frame curframe;

  while (samples_processed < samples_read)
  {
    std::int32_t samples_left = samples_read-samples_processed;
    std::int32_t samples_block = std::min(samples_left, blocksamples);
    double avg_cost=0;
    double avg_used=0;
    for (unsigned ch=0;ch<samples.size();ch++)
    {
      auto [fused,fcost]=AnalyseSparse(std::span{&samples[ch][samples_processed],static_cast<unsigned>(samples_block)});
      avg_cost+=fcost;
      avg_used+=fused;
    }
    avg_cost /= static_cast<double>(samples.size());
    avg_used /= static_cast<double>(samples.size());
    auto block_state=static_cast<std::int32_t>(avg_cost>1.35); // high threshold
    if (opt_.verbose_level>1) {
      std::cout << "  analyse block " << nblock << ' ' << samples_block << " sparse " << block_state << " (" << avg_cost << "," << avg_used << ")\n";
    }

    if (nblock==0)
    {
      curframe.state = block_state;
      curframe.length=samples_block;
      curframe.start=0;
    } else {
      PushState(sub_frames,curframe,min_frame_length,block_state,samples_block);
}

    samples_processed += samples_block;
    nblock++;
  }

  if (curframe.length != 0) {
    PushState(sub_frames,curframe,min_frame_length);
}

  if (samples_processed != samples_read) {
    std::cerr << "  warning: samples_processed != samples_read (" << samples_processed << "," << samples_read << ")\n";
}

  if (opt_.verbose_level>1) { std::cout << "sub_frames\n";
}
  std::int64_t nlen=0;
  for (const auto &frame : sub_frames) {
    if (opt_.verbose_level>1) { std::cout << "  " << frame.start << ' ' << frame.length << ' ' << frame.state << '\n';
}
    nlen+=frame.length;
  }
  if (nlen!=samples_read) {
    std::cerr << "  warning: nlen != samples_read\n";
}
  return sub_frames;
}

std::int32_t Codec::EncodeFile(Wav<AudioFileBase::Mode::Read> &myWav,Sac<AudioFileBase::Mode::Write> &mySac)
{
  std::int32_t max_framesize=opt_.max_framelen*myWav.getSampleRate();

  const std::int32_t numchannels=myWav.getNumChannels();

  FrameCoder myFrame(numchannels,max_framesize,opt_);

  mySac.mcfg.max_framelen = opt_.max_framelen;

  mySac.WriteHeader(myWav);
  std::streampos hdrpos = mySac.file.tellg();
  mySac.WriteMD5(myWav.md5ctx.digest.data());
  myWav.InitFileBuf(max_framesize);

  Timer gtimer;
  Timer ltimer;
  double time_prd=0;
  double time_enc=0;

  gtimer.start();
  std::int32_t samplescoded=0;
  std::int32_t samplestocode=myWav.getNumSamples();
  std::vector<std::vector<std::int32_t>> csamples(myWav.getNumChannels(),std::vector<std::int32_t>(max_framesize));

  while (samplestocode>0) {
      std::int32_t samplesread=myWav.ReadSamples(csamples,max_framesize);

      std::vector<Codec::tsub_frame> sub_frames;
      if (opt_.adapt_block != 0) {
        std::int32_t block_len=myWav.getSampleRate()*3;
        std::int32_t min_frame_len=myWav.getSampleRate()*3;
        sub_frames=Analyse(csamples,block_len,min_frame_len,samplesread);
      } else {
        sub_frames.push_back({0,0,samplesread});
      }

      for (auto &subframe:sub_frames)
      {
        if (opt_.verbose_level != 0) {
          std::cout << "frame " << subframe.start << " state " << subframe.state << " len " << subframe.length << '\n';
}

        for (std::int32_t ch=0;ch<myWav.getNumChannels();ch++) {
          std::copy_n(&csamples[ch][subframe.start],subframe.length,myFrame.samples[ch].data());
}

        myFrame.SetNumSamples(subframe.length);

        ltimer.start();myFrame.Predict();ltimer.stop();time_prd+=ltimer.elapsedS();
        ltimer.start();myFrame.Encode();ltimer.stop();time_enc+=ltimer.elapsedS();
        myFrame.WriteEncoded(mySac);

        samplescoded+=subframe.length;
        PrintProgress(samplescoded,myWav.getNumSamples());
        samplestocode-=subframe.length;
      }
  }
  MD5::Finalize(&myWav.md5ctx);
  gtimer.stop();
  double time_total=gtimer.elapsedS();
  if (time_total>0.)   {
     double rprd=time_prd*100./time_total;
     double renc=time_enc*100./time_total;
     std::cout << "\n  Timing:  pred " << miscUtils::ConvertFixed(rprd,2) << "%, ";
     std::cout << "enc " << miscUtils::ConvertFixed(renc,2) << "%, ";
     std::cout << "misc " << miscUtils::ConvertFixed(100.-rprd-renc,2) << "%" << '\n';
  }
  std::cout << "  MD5:     ";
  for (auto &x : myWav.md5ctx.digest) { std::cout << std::hex << static_cast<std::int32_t>(x);
}
  std::cout << std::dec << '\n';

  std::streampos eofpos = mySac.file.tellg();
  mySac.file.seekg(hdrpos);
  mySac.WriteMD5(myWav.md5ctx.digest.data());
  mySac.file.seekg(eofpos);
  return 0;
}

void Codec::DecodeFile(Sac<AudioFileBase::Mode::Read> &mySac,Wav<AudioFileBase::Mode::Write> &myWav)
{
  const SacBase::sac_cfg &file_cfg=mySac.mcfg;
  myWav.InitFileBuf(static_cast<std::int32_t>(file_cfg.max_framesize));
  mySac.UnpackMetaData(myWav);
  myWav.WriteHeader();

  opt_.max_framelen=file_cfg.max_framelen;
  FrameCoder myFrame(mySac.getNumChannels(),static_cast<std::int32_t>(file_cfg.max_framesize),opt_);

  std::int64_t data_nbytes=0;
  std::int32_t samplestodecode=mySac.getNumSamples();
  std::int32_t samplesdecoded=0;
  while (samplestodecode>0) {
    myFrame.ReadEncoded(mySac);
    myFrame.Decode();
    myFrame.Unpredict();
    data_nbytes += myWav.WriteSamples(myFrame.samples,myFrame.GetNumSamples());

    samplesdecoded+=myFrame.GetNumSamples();
    PrintProgress(samplesdecoded,myWav.getNumSamples());
    samplestodecode-=myFrame.GetNumSamples();
  }
  // pad odd sized data chunk
  if ((static_cast<std::uint64_t>(data_nbytes)&1U) != 0) { myWav.Write(std::vector<std::uint8_t>{0},1);
}
  myWav.WriteHeader();
}
