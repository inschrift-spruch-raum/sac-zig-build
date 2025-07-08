#pragma once

#include <cstdint>
#include <expected>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

class AudioFileErr: std::runtime_error {
  public:
    enum class Err: std::uint8_t {
      OpenFail,
      ReadFail,

      IllegalWav,
      IllegalFormat,

      IllegalSac,      
    };

    explicit AudioFileErr(Err err): std::runtime_error(std::to_string(static_cast<std::int32_t>(err))), err(err){}

    Err err;
};

class AudioFileBase {
  public:
    enum class Mode: std::uint8_t {
      Read,
      Write,
    };

    AudioFileBase();
    AudioFileBase(const AudioFileBase &file);
    AudioFileBase(AudioFileBase &&file) noexcept = default;
    AudioFileBase& operator=(const AudioFileBase& file) = delete;
    AudioFileBase& operator=(AudioFileBase&& file)  noexcept = default;
    ~AudioFileBase() = default;
    
    std::streampos getFileSize() const {return filesize;};
    std::int32_t getNumChannels()const {return numchannels;};
    std::int32_t getSampleRate()const {return samplerate;};
    std::int32_t getBitsPerSample()const {return bitspersample;};
    std::int32_t getKBPS()const {return kbps;};
    void setKBPS(std::int32_t kbps) {this->kbps=kbps;};
    std::int32_t getNumSamples()const {return numsamples;};
    std::streampos readFileSize();
    std::fstream file;
  protected:
    std::streampos filesize;
    std::int32_t samplerate,bitspersample,numchannels,numsamples,kbps;
};

template <AudioFileBase::Mode> class AudioFile : public AudioFileBase {};

template <> class AudioFile<AudioFileBase::Mode::Read> : public AudioFileBase {
  public:
  explicit AudioFile(const std::string &fname);
  
  void Read(std::vector <std::uint8_t>&data,std::size_t len);
  private:
  std::expected<void, AudioFileErr::Err> Open(const std::string &fname);

};

template <> class AudioFile<AudioFileBase::Mode::Write> : public AudioFileBase {
  public:
  AudioFile(const std::string &fname, const AudioFileBase &file);

  void Write(const std::vector <std::uint8_t>&data,std::size_t len);
  private:
  std::expected<void, AudioFileErr::Err> Open(const std::string &fname);

};
