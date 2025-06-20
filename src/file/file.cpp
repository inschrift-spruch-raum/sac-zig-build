#include "file.h"
#include <expected>
#include <iostream>

// Read
AudioFileBase::AudioFileBase() :samplerate(0),bitspersample(0),numchannels(0),numsamples(0),kbps(0){}

AudioFile<AudioFileBase::Mode::Read>::AudioFile(const std::string &fname) : AudioFileBase() {
  auto result = Open(fname);

  if(!result) {
    std::cout << "could not open(read)\n";
    
    throw AudioFileBase::Err::OpenFail;
  }
}

std::expected<void, AudioFileBase::Err> AudioFile<AudioFileBase::Mode::Read>::Open(const std::string &fname) {
    file.open(fname,std::ios_base::in|std::ios_base::binary);

    if (!file.is_open()) {
      return std::unexpected(AudioFileBase::Err::OpenFail);
    }
    
    filesize=readFileSize();
    return {};
}

void AudioFile<AudioFileBase::Mode::Read>::Read(std::vector <uint8_t>&data,size_t len) {
  if (data.size()<len) data.resize(len);
  file.read(reinterpret_cast<char*>(&data[0]),len);
}

// Write
AudioFileBase::AudioFileBase(const AudioFileBase &file)
  :samplerate(file.getSampleRate()),bitspersample(file.getBitsPerSample()),
  numchannels(file.getNumChannels()),numsamples(file.getNumSamples()),kbps(0){}

AudioFile<AudioFileBase::Mode::Write>::AudioFile(const std::string &fname, const AudioFileBase &file) : AudioFileBase(file) {
  auto result = Open(fname);

  if(!result) {
    std::cout << "could not create\n";
    
    throw AudioFileBase::Err::OpenFail;
  }
}

std::expected<void, AudioFileBase::Err> AudioFile<AudioFileBase::Mode::Write>::Open(const std::string &fname) {
  file.open(fname,std::ios_base::out|std::ios_base::binary);
  if (!file.is_open()) return std::unexpected(AudioFileBase::Err::OpenFail);

  return {};
}

void AudioFile<AudioFileBase::Mode::Write>::Write(const std::vector <uint8_t>&data,size_t len) {
  file.write(reinterpret_cast<const char*>(&data[0]),len);
}

// Share
std::streampos AudioFileBase::readFileSize() {
    std::streampos oldpos=file.tellg();
    file.seekg(0,std::ios_base::end);
    std::streampos fsize = file.tellg();
    file.seekg(oldpos);
    return fsize;
}
