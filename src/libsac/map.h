#pragma once // MAP_H

#include "../model/range.h"
#include "../model/counter.h"
#include "../model/mixer.h"
#include "../model/sse.h"
#include <vector>

class MapEncoder {
  const std::int32_t cnt_upd_rate=500;
  const std::int32_t cntsse_upd_rate=300;
  const std::int32_t mix_upd_rate=1000;
  const std::int32_t mixsse_upd_rate=500;
  public:
    MapEncoder(RangeCoderSH &rc,std::vector <bool>&usedl,std::vector <bool>&usedh);
    void Encode();
    void Decode();
  private:
    std::int32_t PredictLow(std::int32_t i);
    std::int32_t PredictHigh(std::int32_t i);
    void Update(std::int32_t bit);
    std::int32_t PredictSSE(std::int32_t p1,std::int32_t ctx);
    void UpdateSSE(std::int32_t bit,std::int32_t ctx);
    RangeCoderSH &rc;
    LinearCounter16 cnt[24];
    LinearCounter16 cctx[256];
    LinearCounter16 *pc1,*pc2,*pc3,*pc4,*px;
    std::vector <NMixLogistic> mixl,mixh;
    NMixLogistic finalmix;
    NMixLogistic *mix;
    SSENL<32> sse[32];
    std::vector <bool>&ul,&uh;
};

class Remap {
  public:
    Remap();
    void Reset();
    double Compare(const Remap &cmap);
    void Analyse(std::vector<std::int32_t> &src,std::int32_t numsamples);
    bool isUsed(std::int32_t val);
    std::int32_t Map2(std::int32_t pred);
    std::int32_t Map(std::int32_t pred,std::int32_t err);
    std::int32_t Unmap(std::int32_t pred,std::int32_t merr);
    std::int32_t scale,vmin,vmax;
    std::vector <bool>usedl,usedh;
    std::vector<std::int32_t> mapl,maph;
};
