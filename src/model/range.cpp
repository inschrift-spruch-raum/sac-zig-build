#include "range.h"

void RangeCoder::Init()
{
  low     = code  = 0;
  range   = std::uint32_t(-1);
  if (decode==1) for (std::uint32_t _=0;_<NUM;_++) (code <<=8) += buf.GetByte();
}

void RangeCoder::Stop()
{
  if (decode==0) for (std::uint32_t _=0;_<NUM;_++) buf.PutByte(low>>24),low<<=8;
}

void RangeCoder::EncodeSymbol(std::uint32_t cumfreq,std::uint32_t freq,std::uint32_t totfreq)
{
  low   += cumfreq * (range /= totfreq);
  range *= freq;
  RANGE_ENC_NORMALIZE(buf, low, range, TOP, BOT);
}

void RangeCoder::DecodeSymbol(std::uint32_t cumfreq,std::uint32_t freq)
{
  low   += cumfreq*range;
  range *= freq;
  RANGE_DEC_NORMALIZE(buf, low, range, code, TOP, BOT);
}

std::uint32_t RangeCoder::DecProb(std::uint32_t totfreq)
{
  std::uint32_t tmp=(code-low) / (range /= totfreq);
  return tmp;
}

void RangeCoder::EncodeBitOne(std::uint32_t p1,const std::int32_t bit)
{
  const std::uint32_t rnew = SCALE_RANGE(range, p1);
  bit ? low += rnew,range-=rnew:range=rnew;
  RANGE_ENC_NORMALIZE(buf, low, range, TOP, BOT);
}

std::int32_t RangeCoder::DecodeBitOne(std::uint32_t p1)
{
  const std::uint32_t rnew=SCALE_RANGE(range, p1);
  std::int32_t bit=(code-low>=rnew);
  bit ? low += rnew,range-=rnew:range=rnew;
  RANGE_DEC_NORMALIZE(buf, low, range, code, TOP, BOT);
  return bit;
}

// binary rangecoder
void RangeCoderSH::Init()
{
  range = 0xFFFFFFFF;
  lowc = FFNum = Cache = code = 0;
  if(decode==1) for (std::uint32_t _=0;_<NUM+1;_++) (code <<=8) += buf.GetByte();
}

void RangeCoderSH::Stop()
{
  if (decode==0) for (std::uint32_t _=0;_<NUM+1;_++) ShiftLow();
}

void RangeCoderSH::EncodeBitOne(std::uint32_t p1,std::int32_t bit)
{
  const std::uint32_t rnew = SCALE_RANGE(range, p1);
  bit ? range-=rnew, lowc+=rnew : range=rnew;
  while(range<TOP) range<<=8,ShiftLow();
}

std::int32_t RangeCoderSH::DecodeBitOne(std::uint32_t p1)
{
  const std::uint32_t rnew = SCALE_RANGE(range, p1);
  std::int32_t bit = (code>=rnew);
  bit ? range-=rnew, code-=rnew : range=rnew;
  while(range<TOP) range<<=8,(code<<=8)+=buf.GetByte();
  return bit;
}

void RangeCoderSH::ShiftLow()
{
  std::uint32_t Carry = std::uint32_t(lowc>>32), low = std::uint32_t(lowc);
  if( low<Thres || Carry )
  {
     buf.PutByte(Cache+Carry);
     for (;FFNum != 0;FFNum--) buf.PutByte(Carry-1);
     Cache = low>>24;
   } else FFNum++;
  lowc = (low<<8);
}
