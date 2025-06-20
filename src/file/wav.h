#pragma once

#include "file.h"
#include "../common/md5.h"

class Chunks {
  public:
    struct tChunk {
      uint32_t id,csize;
      std::vector <uint8_t>data;
    };
    Chunks():metadatasize(0){};
    void Append(uint32_t chunkid,uint32_t chunksize,const uint8_t *data,uint32_t len);
    size_t GetNumChunks() const {return wavchunks.size();};
    uint32_t GetChunkID(int chunk) const {return wavchunks[chunk].id;};
    uint32_t GetChunkSize(int chunk) const {return wavchunks[chunk].csize;};
    size_t GetChunkDataSize(int chunk) const {return wavchunks[chunk].data.size();};
    uint32_t GetMetaDataSize() const {return metadatasize;};
    size_t PackMetaData(std::vector <uint8_t>&data);
    size_t UnpackMetaData(const std::vector <uint8_t>&data);
    std::vector <tChunk> wavchunks;
  private:
    uint32_t metadatasize;
};

class WavBase {
  public:
    WavBase(bool verbose);

    void InitFileBuf(int maxframesize);
    Chunks &GetChunks(){return myChunks;};
    MD5::MD5Context md5ctx;
  protected:
    Chunks myChunks;
    size_t chunkpos;
    std::vector <uint8_t>filebuffer;
    std::streampos datapos,endofdata;
    int byterate,blockalign,samplesleft;
    bool verbose;
};

template<AudioFileBase::Mode> class Wav : public WavBase {};

template<> class Wav<AudioFileBase::Mode::Read> : public WavBase, public AudioFile<AudioFileBase::Mode::Read> {
  public:
    Wav(const std::string &fname, bool verbose=false);
    std::expected<void, AudioFileBase::Err> ReadHeader();
    int ReadSamples(std::vector <std::vector <int32_t>>&data,int samplestoread);
};

template<> class Wav<AudioFileBase::Mode::Write> : public WavBase, public AudioFile<AudioFileBase::Mode::Write> {
  public:
    Wav(const std::string &fname, AudioFile<AudioFileBase::Mode::Read> &file,bool verbose=false);
    void WriteHeader();
    int WriteSamples(const std::vector <std::vector <int32_t>>&data,int samplestowrite);
};
