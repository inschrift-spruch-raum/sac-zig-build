#include "utils.h"
#include <cstdint>
#include <span>


namespace BitUtils
{
  std::uint32_t get32HL(const std::span<const std::uint8_t, 4> buf)
  {
    return(static_cast<std::uint32_t>(buf[3]) + (static_cast<std::uint32_t>(buf[2]) << 8U) +(static_cast<std::uint32_t>(buf[1]) << 16U) + (static_cast<std::uint32_t>(buf[0]) << 24U));
  }
  std::uint16_t get16LH(const std::span<const std::uint8_t, 2> buf)
  {
    return(static_cast<std::uint16_t>(buf[0]) + (static_cast<std::uint16_t>(buf[1]) << 8U));
  }
  std::uint32_t get32LH(const std::span<const std::uint8_t, 4> buf)
  {
    return(static_cast<std::uint32_t>(buf[0]) + (static_cast<std::uint32_t>(buf[1]) << 8U) +(static_cast<std::uint32_t>(buf[2]) << 16U) + (static_cast<std::uint32_t>(buf[3]) << 24U));
  }
  void put16LH(std::span<std::uint8_t, 2> buf,std::uint16_t val)
  {
    buf[0] = val & 0xffU;
    buf[1] = static_cast<uint8_t>(val>>8U) & 0xffU;
  }
  void put32LH(std::span<std::uint8_t, 4> buf,std::uint32_t val)
  {
    buf[0] = val & 0xffU;
    buf[1] = (val>>8U) & 0xffU;
    buf[2] = (val>>16U) & 0xffU;
    buf[3] = (val>>24U) & 0xffU;
  }
  std::string U322Str(std::uint32_t val)
  {
    std::string s;
    for (std::int32_t i=0;i<4;i++) {s+=static_cast<char>(val & 0xffU);val>>=8U;};
    return s;
  }
}  // namespace BitUtils
