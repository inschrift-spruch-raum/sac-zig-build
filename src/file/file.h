#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>
#include <fstream>

class AudioFileBase {
  public:
    enum class Mode {
      Read,
      Write,
    };

    enum class Err {
      OpenFail,
      ReadFail,

      IllegalWav,
      IllegalFormat,

      IllegalSac,      
    };

    AudioFileBase();
    AudioFileBase(const AudioFileBase &file);
    
    std::streampos getFileSize() const {return filesize;};
    int getNumChannels()const {return numchannels;};
    int getSampleRate()const {return samplerate;};
    int getBitsPerSample()const {return bitspersample;};
    int getKBPS()const {return kbps;};
    void setKBPS(int kbps) {this->kbps=kbps;};
    int getNumSamples()const {return numsamples;};
    std::streampos readFileSize();
    std::fstream file;
  protected:
    std::streampos filesize;
    int samplerate,bitspersample,numchannels,numsamples,kbps;
};

template <AudioFileBase::Mode> class AudioFile : public AudioFileBase {};

template <> class AudioFile<AudioFileBase::Mode::Read> : public AudioFileBase {
  public:
  AudioFile(const std::string &fname);
  
  std::expected<void, AudioFileBase::Err> Open(const std::string &fname);
  void Read(std::vector <uint8_t>&data,size_t len);
};

template <> class AudioFile<AudioFileBase::Mode::Write> : public AudioFileBase {
  public:
  AudioFile(const std::string &fname, const AudioFileBase &file);

  std::expected<void, AudioFileBase::Err> Open(const std::string &fname);
  void Write(const std::vector <uint8_t>&data,size_t len);
};
