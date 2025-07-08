#ifndef BUFIO_H
#define BUFIO_H

#include <vector>
#include <cstdint>

class BufIO {
  public:
      BufIO():buf(1024){Reset();};
      explicit BufIO(std::int32_t initsize):buf(initsize){Reset();};
      void Reset(){bufpos=0;};
      void PutByte(std::int32_t val)
      {
        if (bufpos>=buf.size()) buf.resize(buf.size()*2);
        buf[bufpos++]=val;
      }
      std::int32_t GetByte() {
        if (bufpos>=buf.size()) return -1;
        else return buf[bufpos++];
      }
      std::size_t GetBufPos(){return bufpos;};
      std::vector <std::uint8_t> &GetBuf(){return buf;};
  private:
     std::size_t bufpos;
     std::vector <std::uint8_t>buf;
};

#endif
