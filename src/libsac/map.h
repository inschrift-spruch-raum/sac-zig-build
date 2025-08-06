#pragma once // MAP_H

#include "../model/counter.h"
#include "../model/mixer.h"
#include "../model/range.h"
#include "../model/sse.h"

#include <array>
#include <cstddef>
#include <vector>

class MapEncoder {
  static constexpr std::int32_t cnt_upd_rate = 500;
  static constexpr std::int32_t cntsse_upd_rate = 300;
  static constexpr std::int32_t mix_upd_rate = 1000;
  static constexpr std::int32_t mixsse_upd_rate = 500;

public:
  MapEncoder(
    RangeCoderSH& rc, std::vector<bool>& usedl, std::vector<bool>& usedh
  );
  void Encode();
  void Decode();

private:
  std::int32_t PredictLow(std::size_t i);
  std::int32_t PredictHigh(std::size_t i);
  void Update(std::int32_t bit);
  std::int32_t PredictSSE(std::int32_t p1, std::int32_t ctx);
  void UpdateSSE(std::int32_t bit, std::int32_t ctx);
  RangeCoderSH& rc;
  std::array<LinearCounter16, 24> cnt;
  std::array<LinearCounter16, 256> cctx;
  LinearCounter16 *pc1, *pc2, *pc3, *pc4, *px;
  std::vector<NMixLogistic> mixl, mixh;
  NMixLogistic finalmix;
  NMixLogistic* mix;
  std::array<SSENL<32>, 32> sse;
  std::vector<bool>&ul, &uh;
};

class Remap {
public:
  Remap();
  void Reset();
  double Compare(const Remap& cmap);
  void Analyse(std::vector<std::int32_t>& src, std::int32_t numsamples);
  bool isUsed(std::int32_t val);
  std::int32_t Map2(std::int32_t pred);
  std::int32_t Map(std::int32_t pred, std::int32_t err);
  std::int32_t Unmap(std::int32_t pred, std::int32_t merr);
  std::int32_t scale, vmin, vmax;
  std::vector<bool> usedl, usedh;
  std::vector<std::int32_t> mapl, maph;
};
