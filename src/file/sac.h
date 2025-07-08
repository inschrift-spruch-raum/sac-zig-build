#pragma once

#include "file.h"
#include "wav.h"

#include <iostream>

class SacBase {
  public:
  struct sac_cfg {
    std::uint8_t max_framelen=0;

    std::uint32_t max_framesize=0;
    std::uint32_t metadatasize=0;
  } mcfg;

  

  template <AudioFileBase::Mode Mode>
  std::int32_t UnpackMetaData(Wav<Mode> &myWav) {
    std::size_t unpackedbytes=myWav.GetChunks().UnpackMetaData(metadata);
    if (mcfg.metadatasize!=unpackedbytes) {std::cerr << "  warning: unpackmetadata mismatch\n";return 1;}
    return 0;
  }

  std::vector<std::uint8_t> metadata;
};

template <AudioFileBase::Mode> class Sac : public SacBase {};

template <> class Sac<AudioFileBase::Mode::Read> : public SacBase, public AudioFile<AudioFileBase::Mode::Read> {
  public:
    explicit Sac(const std::string &fname);

    std::expected<void, AudioFileErr::Err> ReadHeader();
    void ReadMD5(std::uint8_t digest[16]);
};

template <> class Sac<AudioFileBase::Mode::Write> : public SacBase, public AudioFile<AudioFileBase::Mode::Write> {
  public:
    Sac(const std::string &fname, AudioFile<AudioFileBase::Mode::Read> &file);
    
    void WriteHeader(Wav<AudioFileBase::Mode::Read> &myWav);
    void WriteMD5(std::uint8_t digest[16]);
};
