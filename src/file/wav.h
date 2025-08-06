#pragma once

#include "../common/md5.h"
#include "file.h"

#include <span>

class Chunks {
public:
  struct tChunk {
    std::uint32_t id, csize;
    std::vector<std::uint8_t> data;
  };

  Chunks() = default;
  void Append(
    std::uint32_t chunkid, std::uint32_t chunksize,
    std::span<const std::uint8_t> data
  );

  std::size_t GetNumChunks() const { return wavchunks.size(); };

  std::uint32_t GetChunkID(std::int32_t chunk) const {
    return wavchunks[chunk].id;
  };

  std::uint32_t GetChunkSize(std::int32_t chunk) const {
    return wavchunks[chunk].csize;
  };

  std::size_t GetChunkDataSize(std::int32_t chunk) const {
    return wavchunks[chunk].data.size();
  };

  std::uint32_t GetMetaDataSize() const { return metadatasize; };

  std::size_t PackMetaData(std::vector<std::uint8_t>& data);
  std::size_t UnpackMetaData(const std::vector<std::uint8_t>& data);
  std::vector<tChunk> wavchunks;

private:
  std::uint32_t metadatasize{0};
};

class WavBase {
public:
  explicit WavBase(bool verbose);

  void InitFileBuf(std::int32_t maxframesize);

  Chunks& GetChunks() { return myChunks; };

  MD5::MD5Context md5ctx;

protected:
  Chunks myChunks;
  std::size_t chunkpos;
  std::vector<std::uint8_t> filebuffer;
  std::streampos datapos, endofdata;
  std::int32_t byterate, blockalign, samplesleft;
  bool verbose;
};

template<AudioFileBase::Mode> class Wav: public WavBase {};

template<>
class Wav<AudioFileBase::Mode::Read>:
  public WavBase,
  public AudioFile<AudioFileBase::Mode::Read> {
public:
  explicit Wav(const std::string& fname, bool verbose = false);
  std::expected<void, AudioFileErr::Err> ReadHeader();
  std::int32_t ReadSamples(
    std::vector<std::vector<std::int32_t>>& data, std::int32_t samplestoread
  );
};

template<>
class Wav<AudioFileBase::Mode::Write>:
  public WavBase,
  public AudioFile<AudioFileBase::Mode::Write> {
public:
  Wav(
    const std::string& fname, AudioFile<AudioFileBase::Mode::Read>& file,
    bool verbose = false
  );
  void WriteHeader();
  std::int32_t WriteSamples(
    const std::vector<std::vector<std::int32_t>>& data,
    std::int32_t samplestowrite
  );
};
