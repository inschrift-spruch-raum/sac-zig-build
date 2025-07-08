#include "file.h"

#include <cstdint>
#include <expected>
#include <ios>
#include <iostream>

// Read
AudioFileBase::AudioFileBase():
  samplerate(0),
  bitspersample(0),
  numchannels(0),
  numsamples(0),
  kbps(0) {}

AudioFile<AudioFileBase::Mode::Read>::AudioFile(const std::string& fname) {
  auto result = Open(fname);

  if(!result) {
    std::cout << "could not open(read)\n";

    throw AudioFileErr(AudioFileErr::Err::OpenFail);
  }
}

std::expected<void, AudioFileErr::Err>
AudioFile<AudioFileBase::Mode::Read>::Open(const std::string& fname) {
  file.open(
    fname, static_cast<std::ios_base::openmode>(
             static_cast<uint8_t>(std::ios_base::in)
             | static_cast<uint8_t>(std::ios_base::binary)
           )
  );

  if(!file.is_open()) { return std::unexpected(AudioFileErr::Err::OpenFail); }

  filesize = readFileSize();
  return {};
}

void AudioFile<AudioFileBase::Mode::Read>::Read(
  std::vector<uint8_t>& data, size_t len
) {
  if(data.size() < len) { data.resize(len); }
  file.read(
    reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(len)
  );
}

// Write
AudioFileBase::AudioFileBase(const AudioFileBase& file):
  samplerate(file.getSampleRate()),
  bitspersample(file.getBitsPerSample()),
  numchannels(file.getNumChannels()),
  numsamples(file.getNumSamples()),
  kbps(0) {}

AudioFile<AudioFileBase::Mode::Write>::AudioFile(
  const std::string& fname, const AudioFileBase& file
):
  AudioFileBase(file) {
  auto result = Open(fname);

  if(!result) {
    std::cout << "could not create\n";

    throw AudioFileErr(AudioFileErr::Err::OpenFail);
  }
}

std::expected<void, AudioFileErr::Err>
AudioFile<AudioFileBase::Mode::Write>::Open(const std::string& fname) {
  file.open(
    fname, static_cast<std::ios_base::openmode>(
             static_cast<uint8_t>(std::ios_base::out)
             | static_cast<uint8_t>(std::ios_base::binary)
           )
  );
  if(!file.is_open()) { return std::unexpected(AudioFileErr::Err::OpenFail); }

  return {};
}

void AudioFile<AudioFileBase::Mode::Write>::Write(
  const std::vector<uint8_t>& data, size_t len
) {
  file.write(
    reinterpret_cast<const char*>(data.data()),
    static_cast<std::streamsize>(len)
  );
}

// Share
std::streampos AudioFileBase::readFileSize() {
  std::streampos oldpos = file.tellg();
  file.seekg(0, std::ios_base::end);
  std::streampos fsize = file.tellg();
  file.seekg(oldpos);
  return fsize;
}
