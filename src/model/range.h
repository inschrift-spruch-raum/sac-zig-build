#pragma once // RANGE_H

#include "../common/bufio.h"
#include "model.h"
#include <cstdint>
#include <functional>

class RangeCoderBase {
  public:
    explicit RangeCoderBase(BufIO &buf,std::int32_t dec=0):buf(buf),decode(dec){};
    void SetDecode(){decode=1;};
    void SetEncode(){decode=0;};
  protected:
    BufIO &buf;
    std::int32_t decode;
};

inline std::uint32_t SCALE_RANGE(std::uint32_t &range, std::uint32_t &p1) {return ((std::uint64_t(range)*((PSCALE-p1)<<(32-PBITS)))>>32);}

inline void RANGE_ENC_NORMALIZE(BufIO &buf, std::uint32_t &low, std::uint32_t &range, std::uint32_t TOP, std::uint32_t BOT) { 
  while ((low ^ (low+range))<TOP || (range<BOT && ((range= -(std::int32_t)low & (BOT-1)),1))) {
    buf.PutByte(low>>24);
    range<<=8;
    low<<=8;
  }
}

inline void RANGE_DEC_NORMALIZE(BufIO &buf, std::uint32_t &low, std::uint32_t &range, std::uint32_t &code, std::uint32_t TOP, std::uint32_t BOT)  {
  while ((low ^ (low+range))<TOP || (range<BOT && ((range= -(std::int32_t)low & (BOT-1)),1))) {
    (code<<=8)+=buf.GetByte();
    range<<=8;
    low<<=8;
  }
}

// Carryless RangeCoder
// derived from Dimitry Subbotin (public domain)
class RangeCoder : public RangeCoderBase
{
  enum : std::uint32_t {NUM=4,TOP=0x01000000U,BOT=0x00010000U};
  public:
    using RangeCoderBase::RangeCoderBase;
    void Init();
    void Stop();
    void EncodeSymbol(std::uint32_t low,std::uint32_t freq,std::uint32_t tot);
    void DecodeSymbol(std::uint32_t low,std::uint32_t freq);
    void EncodeBitOne(std::uint32_t p1,std::int32_t bit);
    std::int32_t  DecodeBitOne(std::uint32_t p1);
    std::uint32_t DecProb(std::uint32_t totfreq);
  protected:
    std::uint32_t low,range,code;
};


// Binary RangeCoder with Carry and 64-bit low
// derived from rc_v3 by Eugene Shelwien
class RangeCoderSH : public RangeCoderBase {
  enum : std::uint32_t { NUM=4,TOP=0x01000000U,Thres=0xFF000000U};
  public:
    using RangeCoderBase::RangeCoderBase;
    void Init();
    void Stop();
    void EncodeBitOne(std::uint32_t p1,std::int32_t bit);
    std::int32_t  DecodeBitOne(std::uint32_t p1);

    std::function<void(std::uint32_t,std::int32_t)> encode_p1 = [this](std::uint32_t p1,std::int32_t bit) {return EncodeBitOne(p1,bit);}; // stupid C++
    std::function<std::int32_t(std::uint32_t)> decode_p1 = [this](std::uint32_t p1) {return DecodeBitOne(p1);};
  protected:
    void ShiftLow();
    std::uint32_t range,code,FFNum,Cache;
    std::uint64_t lowc;
};
