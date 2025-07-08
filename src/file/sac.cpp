#include "sac.h"
#include "../common/utils.h"
#include "file.h"
#include <array>
#include <cstdint>
#include <expected>

// Read
Sac<AudioFileBase::Mode::Read>::Sac(const std::string &fname) try : AudioFile(fname){
  auto Result = ReadHeader();
  
  if (!Result) {
    std::cout << "warning: input is not a valid .sac file\n";
    
    throw AudioFileErr(AudioFileErr::Err::IllegalSac);
  }
} catch (AudioFileErr&) {
  throw;
}

std::expected<void, AudioFileErr::Err> Sac<AudioFileBase::Mode::Read>::ReadHeader() {
  std::array<std::uint8_t, 32> buf{};
  file.read(reinterpret_cast<char*>(buf.data()),22);
  
  if (buf[0]!='S' || buf[1]!='A' || buf[2]!='C' || buf[3]!='2') {
    return std::unexpected(AudioFileErr::Err::IllegalSac);
  }

  numchannels=BitUtils::get16LH(&buf[4]);
  samplerate=static_cast<std::int32_t>(BitUtils::get32LH(&buf[6]));
  bitspersample=BitUtils::get16LH(&buf[10]);
  numsamples=static_cast<std::int32_t>(BitUtils::get32LH(&buf[12]));
  mcfg.max_framelen=buf[16];
  mcfg.metadatasize=BitUtils::get32LH(&buf[18]);
  Read(metadata,mcfg.metadatasize);
  mcfg.max_framesize=samplerate*static_cast<std::uint32_t>(mcfg.max_framelen);
  return {};
}

void Sac<AudioFileBase::Mode::Read>::ReadMD5(std::uint8_t digest[16]) {
  file.read(reinterpret_cast<char*>(digest), 16);
}

// Write
Sac<AudioFileBase::Mode::Write>::Sac(const std::string &fname, AudioFile<AudioFileBase::Mode::Read> &file) try : 
AudioFile(fname, file) {} catch (AudioFileErr&) {
  throw;
}

void Sac<AudioFileBase::Mode::Write>::WriteHeader(Wav<AudioFileBase::Mode::Read> &myWav) {
  Chunks &myChunks=myWav.GetChunks();
  std::array<std::uint8_t, 32> buf{};
  std::vector <std::uint8_t>metadata;
  buf[0]='S';
  buf[1]='A';
  buf[2]='C';
  buf[3]='2';
  BitUtils::put16LH(&buf[4],numchannels);
  BitUtils::put32LH(&buf[6],samplerate);
  BitUtils::put16LH(&buf[10],bitspersample);
  BitUtils::put32LH(&buf[12],numsamples);
  buf[16] = mcfg.max_framelen;
  buf[17] = 0;

  // write wav meta data
  const std::uint32_t metadatasize=myChunks.GetMetaDataSize();
  BitUtils::put32LH(&buf[18],metadatasize);
  file.write(reinterpret_cast<char*>(buf.data()),22);
  if (myChunks.PackMetaData(metadata)!=metadatasize) { std::cerr << "  warning: metadatasize mismatch\n";}
  Write(metadata,metadatasize);
}

void Sac<AudioFileBase::Mode::Write>::WriteMD5(std::uint8_t digest[16]) {
  file.write(reinterpret_cast<char*>(digest),16);
}
