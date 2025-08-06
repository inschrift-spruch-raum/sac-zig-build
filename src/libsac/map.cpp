#include "map.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>

MapEncoder::MapEncoder(
  RangeCoderSH& rc, std::vector<bool>& usedl, std::vector<bool>& usedh
):
  rc(rc),
  mixl(4, NMixLogistic(5)),
  mixh(4, NMixLogistic(5)),
  finalmix(2),
  ul(usedl),
  uh(usedh) {}

std::int32_t MapEncoder::PredictLow(std::size_t i) {
  std::uint32_t ctx1 = static_cast<std::uint32_t>(ul[i - 1]);
  std::int32_t ctx2 = static_cast<std::int32_t>(uh[i - 1]);
  std::uint32_t ctx3 = i > 1 ? static_cast<std::uint32_t>(ul[i - 2]) : 0;

  pc1 = &cnt[ctx1];
  pc2 = &cnt[2 + ctx2];
  pc3 = &cnt[4 + (ctx1 << 1U) + ctx3];
  pc4 = &cnt[8 + (ctx1 << 1U) + ctx2];

  std::int32_t sctx = static_cast<std::int32_t>(ul[i - 1]);
  if(i > 1) {
    sctx +=
      static_cast<std::int32_t>(static_cast<std::uint32_t>(ul[i - 2]) << 1U);
  }
  if(i > 2) {
    sctx +=
      static_cast<std::int32_t>(static_cast<std::uint32_t>(ul[i - 3]) << 2U);
  }
  if(i > 3) {
    sctx +=
      static_cast<std::int32_t>(static_cast<std::uint32_t>(ul[i - 4]) << 3U);
  }
  px = &cctx[sctx];

  mix = &mixl[ctx1 + (ctx3 << 1U)];
  std::vector<std::int32_t> p = {pc1->p1, pc2->p1, pc3->p1, pc4->p1, px->p1};
  return mix->Predict(p);
}

std::int32_t MapEncoder::PredictHigh(std::size_t i) {
  std::uint32_t ctx1 = static_cast<std::uint32_t>(uh[i - 1]);
  std::int32_t ctx2 = static_cast<std::int32_t>(ul[i]);
  std::uint32_t ctx3 = i > 1 ? static_cast<std::uint32_t>(uh[i - 2]) : 0;
  // std::int32_t n=0;
  pc1 = &cnt[12 + ctx1];
  pc2 = &cnt[12 + 2 + ctx2];
  pc3 = &cnt[12 + 4 + (ctx1 << 1U) + ctx3];
  pc4 = &cnt[12 + 8 + (ctx1 << 1U) + ctx2];

  std::int32_t sctx = static_cast<std::int32_t>(uh[i - 1]);
  if(i > 1) {
    sctx +=
      static_cast<std::int32_t>(static_cast<std::uint32_t>(uh[i - 2]) << 1U);
  }
  if(i > 2) {
    sctx +=
      static_cast<std::int32_t>(static_cast<std::uint32_t>(uh[i - 3]) << 2U);
  }
  if(i > 3) {
    sctx +=
      static_cast<std::int32_t>(static_cast<std::uint32_t>(uh[i - 4]) << 3U);
  }
  px = &cctx[32 + sctx];
  mix = &mixh[ctx1 + (ctx3 << 1U)];
  std::vector<std::int32_t> p = {pc1->p1, pc2->p1, pc3->p1, pc4->p1, px->p1};
  return mix->Predict(p);
}

void MapEncoder::Update(std::int32_t bit) {
  pc1->update(bit, cnt_upd_rate);
  pc2->update(bit, cnt_upd_rate);
  pc3->update(bit, cnt_upd_rate);
  pc4->update(bit, cnt_upd_rate);
  px->update(bit, cnt_upd_rate);
  mix->Update(bit, mix_upd_rate);
}

std::int32_t MapEncoder::PredictSSE(std::int32_t p1, std::int32_t ctx) {
  std::vector<std::int32_t> vp = {sse[ctx].Predict(p1), p1};
  return finalmix.Predict(vp);
}

void MapEncoder::UpdateSSE(std::int32_t bit, std::int32_t ctx) {
  sse[ctx].Update(bit, cntsse_upd_rate);
  finalmix.Update(bit, mixsse_upd_rate);
}

void MapEncoder::Encode() {
  for(std::size_t i = 1; i <= 1U << 15U; i++) {
    std::int32_t bit = static_cast<std::int32_t>(ul[i]);

    rc.EncodeBitOne(PredictSSE(PredictLow(i), 0), bit);
    Update(bit);
    UpdateSSE(bit, 0);

    bit = static_cast<std::int32_t>(uh[i]);
    rc.EncodeBitOne(PredictSSE(PredictHigh(i), 0), bit);
    Update(bit);
    UpdateSSE(bit, 0);
  }
}

void MapEncoder::Decode() {
  for(std::size_t i = 1; i <= 1U << 15U; i++) {
    std::int32_t bit = rc.DecodeBitOne(PredictSSE(PredictLow(i), 0));
    Update(bit);
    ul[i] = (bit != 0);
    UpdateSSE(bit, 0);

    bit = rc.DecodeBitOne(PredictSSE(PredictHigh(i), 0));
    Update(bit);
    uh[i] = (bit != 0);
    UpdateSSE(bit, 0);
  }
}

Remap::Remap(): scale(1U << 15U), usedl(scale + 1), usedh(scale + 1) {}

void Remap::Reset() {
  std::fill(begin(usedl), end(usedl), 0);
  std::fill(begin(usedh), end(usedh), 0);
  vmin = vmax = 0;
}

double Remap::Compare(const Remap& cmap) {
  std::int32_t diff = 0;
  for(std::int32_t i = 1; i <= scale; i++) {
    if(usedl[i] != cmap.usedl[i]) { diff++; }
    if(usedh[i] != cmap.usedh[i]) { diff++; }
  }
  return diff * 100. / static_cast<double>(2 * scale);
}

void Remap::Analyse(std::vector<std::int32_t>& src, std::int32_t numsamples) {
  for(std::int32_t i = 0; i < numsamples; i++) {
    std::int32_t val = src[i];
    if(val > 0) {
      if(val > scale) {
        std::cout << "val too large: " << val << '\n';
      } else {
        vmax = std::max(val, vmax);
        usedh[val] = true;
      }
    } else if(val < 0) {
      val = (-val);
      if(val > scale) {
        std::cout << "val too large: " << val << '\n';
      } else {
        vmin = std::max(val, vmin);
        usedl[val] = true;
      }
    }
  }
  mapl.resize((1U << 15U) + 1);
  maph.resize((1U << 15U) + 1);
  std::int32_t j = 1;
  for(std::size_t i = 1; i <= (1U << 15U); i++) {
    mapl[i] = j;
    if(usedl[i]) { j++; };
  }
  j = 1;
  for(std::size_t i = 1; i <= (1U << 15U); i++) {
    maph[i] = j;
    if(usedh[i]) { j++; };
  }
}

bool Remap::isUsed(std::int32_t val) {
  if(val > scale) { return false; }
  if(val < -scale) { return false; }
  if(val > 0) { return usedh[val]; }
  if(val < 0) { return usedl[-val]; }
  return true;
}

std::int32_t Remap::Map2(std::int32_t pred) {
  if(pred > 0) { return maph[pred]; }
  if(pred < 0) { return -mapl[-pred]; }
  return 0;
}

std::int32_t Remap::Map(std::int32_t pred, std::int32_t err) {
  std::int32_t sgn = 1;
  if(err == 0) { return 0; }
  if(err < 0) {
    err = -err;
    sgn = -1;
  };

  std::int32_t merr = 0;
  for(std::int32_t i = 1; i <= err; i++) {
    if(isUsed(pred + (sgn * i))) { merr++; }
  }
  return sgn * merr;
}

std::int32_t Remap::Unmap(std::int32_t pred, std::int32_t merr) {
  std::int32_t sgn = 1;
  if(merr == 0) { return 0; }
  if(merr < 0) {
    merr = -merr;
    sgn = -1;
  };

  std::int32_t err = 1;
  std::int32_t terr = 0;
  while(true) {
    if(isUsed(pred + (sgn * err))) { terr++; }
    if(terr == merr) { break; }
    err++;
  }
  return sgn * err;
}
