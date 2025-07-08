#include "utils.h"


namespace BitUtils
{
  std::uint32_t get32HL(const std::uint8_t *buf)
  {
    return((std::uint32_t)buf[3] + ((std::uint32_t)buf[2] << 8) +((std::uint32_t)buf[1] << 16) + ((std::uint32_t)buf[0] << 24));
  }
  std::uint16_t get16LH(const std::uint8_t *buf)
  {
    return((std::uint16_t)buf[0] + ((std::uint16_t)buf[1] << 8));
  }
  std::uint32_t get32LH(const std::uint8_t *buf)
  {
    return((std::uint32_t)buf[0] + ((std::uint32_t)buf[1] << 8) +((std::uint32_t)buf[2] << 16) + ((std::uint32_t)buf[3] << 24));
  }
  void put16LH(std::uint8_t *buf,std::uint16_t val)
  {
    buf[0] = val & 0xff;
    buf[1] = (val>>8) & 0xff;
  }
  void put32LH(std::uint8_t *buf,std::uint32_t val)
  {
    buf[0] = val & 0xff;
    buf[1] = (val>>8) & 0xff;
    buf[2] = (val>>16) & 0xff;
    buf[3] = (val>>24) & 0xff;
  }
  std::string U322Str(std::uint32_t val)
  {
    std::string s;
    for (std::int32_t i=0;i<4;i++) {s+=(char)(val & 0xff);val>>=8;};
    return s;
  }
}
