#include "wav.h"

#include "../common/utils.h"
#include "file.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <span>

namespace {
  std::int32_t word_align(std::int32_t numbytes) {
    return ((static_cast<std::int32_t>(
              static_cast<std::uint32_t>(numbytes) & 1U
            ))
            != 0)
             ? (numbytes + 1)
             : numbytes;
  }
} // namespace

void Chunks::Append(
  std::uint32_t chunkid, std::uint32_t chunksize,
  const std::span<const std::uint8_t> data
) {
  tChunk chunk;
  chunk.id = chunkid;
  chunk.csize = chunksize;
  if(data.size() != 0U) {
    chunk.data.resize(data.size());
    std::ranges::copy(data, chunk.data.begin());
  }
  wavchunks.push_back(chunk);
  metadatasize += (8 + data.size());
}

std::size_t Chunks::PackMetaData(std::vector<std::uint8_t>& data) {
  data.resize(metadatasize);
  std::size_t ofs = 0;
  for(std::size_t i = 0; i < GetNumChunks(); i++) {
    const tChunk& wavchunk = wavchunks[i];
    BitUtils::put32LH(std::span<std::uint8_t, 4>(&data[ofs], 4), wavchunk.id);
    ofs += 4;
    BitUtils::put32LH(
      std::span<std::uint8_t, 4>(&data[ofs], 4), wavchunk.csize
    );
    ofs += 4;
    std::copy(
      begin(wavchunk.data), end(wavchunk.data),
      begin(data) + static_cast<std::int32_t>(ofs)
    );
    ofs += wavchunk.data.size();
  }
  return ofs;
}

std::size_t Chunks::UnpackMetaData(const std::vector<std::uint8_t>& data) {
  std::size_t ofs = 0;
  while(ofs < data.size()) {
    std::uint32_t chunkid =
      BitUtils::get32LH(std::span<const std::uint8_t, 4>(&data[ofs], 4));
    ofs += 4;
    std::uint32_t chunksize =
      BitUtils::get32LH(std::span<const std::uint8_t, 4>(&data[ofs], 4));
    ofs += 4;
    if(chunkid == 0x46464952) {
      Append(chunkid, chunksize, std::span<const std::uint8_t>{&data[ofs], 4});
      ofs += 4;
    } else if(chunkid == 0x61746164) {
      Append(chunkid, chunksize, std::span<const std::uint8_t, 0>{});
    } else {
      const std::uint32_t writesize =
        word_align(static_cast<std::int32_t>(chunksize));
      Append(
        chunkid, chunksize, std::span<const std::uint8_t>{&data[ofs], writesize}
      );
      ofs += writesize;
    };
  }
  return ofs;
}

// Base
WavBase::WavBase(bool verbose):
  md5ctx(0),
  chunkpos(0),
  datapos(0),
  endofdata(0),
  byterate(0),
  blockalign(0),
  samplesleft(0),
  verbose(verbose) {
  MD5::Init(&md5ctx);
};

// Read
Wav<AudioFileBase::Mode::Read>::Wav(const std::string& fname, bool verbose) try:
  WavBase(verbose),
  AudioFile(fname) {
  auto Result = ReadHeader();

  if(!Result) {
    std::cout << "warning: input is not a valid .wav file\n";

    throw AudioFileErr(AudioFileErr::Err::IllegalWav);
  }
} catch(AudioFileErr&) { throw; }

std::expected<void, AudioFileErr::Err>
Wav<AudioFileBase::Mode::Read>::ReadHeader() {
  bool seektodatapos = true;
  std::array<std::uint8_t, 40> buf{};
  std::vector<std::uint8_t> vbuf;
  std::uint32_t chunkid = 0;
  std::uint32_t chunksize = 0;

  file.read(
    reinterpret_cast<char*>(buf.data()), 12
  ); // read 'RIFF' chunk descriptor
  chunkid = BitUtils::get32LH(std::span<std::uint8_t, 4>(buf.data(), 4));
  chunksize = BitUtils::get32LH(std::span<std::uint8_t, 4>(&buf[4], 4));

  // do we have a wave file?
  if(chunkid == 0x46464952
     && BitUtils::get32LH(std::span<std::uint8_t, 4>(&buf[8], 4))
          == 0x45564157) {
    myChunks.Append(
      chunkid, chunksize, std::span<const std::uint8_t>{&buf[8], 4}
    );
    while(true) {
      file.read(reinterpret_cast<char*>(buf.data()), 8);
      if(!file) {
        std::cout << "could not read!\n";
        return std::unexpected(AudioFileErr::Err::ReadFail);
      };

      chunkid = BitUtils::get32LH(std::span<std::uint8_t, 4>(buf.data(), 4));
      chunksize = BitUtils::get32LH(std::span<std::uint8_t, 4>(&buf[4], 4));
      if(chunkid == 0x020746d66) { // read 'fmt ' chunk
        if(chunksize != 16 && chunksize != 18 && chunksize != 40) {
          std::cerr << "warning: invalid fmt-chunk size (" << chunksize
                    << ")\n";
          return std::unexpected(AudioFileErr::Err::IllegalWav);
        }
        file.read(reinterpret_cast<char*>(buf.data()), chunksize);
        myChunks.Append(
          chunkid, chunksize,
          std::span<const std::uint8_t>{buf.data(), chunksize}
        );

        std::int32_t audioformat =
          BitUtils::get16LH(std::span<std::uint8_t, 2>(buf.data(), 2));
        numchannels = BitUtils::get16LH(std::span<std::uint8_t, 2>(&buf[2], 2));
        samplerate = static_cast<std::int32_t>(
          BitUtils::get32LH(std::span<std::uint8_t, 4>(&buf[4], 4))
        );
        byterate = static_cast<std::int32_t>(
          BitUtils::get32LH(std::span<std::uint8_t, 4>(&buf[8], 4))
        );
        blockalign = BitUtils::get16LH(std::span<std::uint8_t, 2>(&buf[12], 2));
        bitspersample =
          BitUtils::get16LH(std::span<std::uint8_t, 2>(&buf[14], 2));
        if(chunksize >= 18) {
          std::int32_t cbsize =
            BitUtils::get16LH(std::span<std::uint8_t, 2>(&buf[16], 2));
          if(verbose) {
            std::cout << "  WAVE-Ext (" << cbsize << " Bytes)" << '\n';
          }
          if(cbsize >= 22) {
            std::int32_t valid_bitspersample =
              BitUtils::get16LH(std::span<std::uint8_t, 2>(&buf[18], 2));
            auto channel_mask = static_cast<std::int32_t>(
              BitUtils::get32LH(std::span<std::uint8_t, 4>(&buf[20], 4))
            );
            std::int32_t data_fmt =
              BitUtils::get16LH(std::span<std::uint8_t, 2>(&buf[24], 2));
            if(verbose) {
              std::cout << "  audio format="
                        << std::format("{:#x}", audioformat);
              std::cout << ",channel mask="
                        << std::format("{:#x}", channel_mask);
              std::cout << ",valid bps=" << valid_bitspersample;
              std::cout << ",data format=" << data_fmt << '\n';
            }
            bitspersample = valid_bitspersample;
            audioformat = data_fmt;
          }
        }
        kbps = (samplerate * numchannels * bitspersample) / 1000;
        if(audioformat != 1) {
          std::cerr << "warning: only PCM Format supported\n";
          return std::unexpected(AudioFileErr::Err::IllegalFormat);
        };

      } else if(chunkid == 0x61746164) { // 'data' chunk
        myChunks.Append(chunkid, chunksize, std::span<std::uint8_t, 0>{});
        datapos = file.tellg();

        numsamples = static_cast<std::int32_t>(chunksize) / blockalign;
        samplesleft = numsamples;

        endofdata =
          datapos
          + std::streampos(word_align(static_cast<std::int32_t>(chunksize)));
        // std::cout << endofdata << ' ' << filesize << '\n';
        if(endofdata >= filesize) { // if data chunk is last chunk, break
          if(endofdata > filesize) {
            numsamples = static_cast<std::int32_t>(
              (filesize - datapos) / static_cast<std::int64_t>(blockalign)
            );
            samplesleft = numsamples;
            std::cerr << "  warning: endofdata>filesize\n";
            std::cerr << "  numsamples: " << numsamples << '\n';
          }
          seektodatapos = false;
          break;
        }
        std::int64_t pos = file.tellg();
        file.seekg(pos + chunksize);

      } else { // read remaining chunks
        const std::uint32_t readsize =
          word_align(static_cast<std::int32_t>(chunksize));
        Read(vbuf, readsize);
        myChunks.Append(
          chunkid, chunksize,
          std::span<const std::uint8_t>{vbuf.data(), readsize}
        );
      }
      if(file.tellg() == getFileSize()) { break; }
    }
  } else {
    return std::unexpected(AudioFileErr::Err::IllegalWav);
  }

  if(verbose) {
    std::cout << "\nNumber of chunks: " << myChunks.GetNumChunks() << '\n';
    for(std::size_t i = 0; i < myChunks.GetNumChunks(); i++) {
      std::cout << "  Chunk" << std::setw(2) << (i + 1) << ": '"
                << BitUtils::U322Str(
                     myChunks.GetChunkID(static_cast<std::int32_t>(i))
                   )
                << "' " << myChunks.GetChunkSize(static_cast<std::int32_t>(i))
                << " Bytes\n";
    }
    std::cout << "  Metadatasize: " << myChunks.GetMetaDataSize() << " Bytes\n";
  }
  if(seektodatapos) {
    file.seekg(datapos);
    seektodatapos = false;
  };
  return {};
}

std::int32_t Wav<AudioFileBase::Mode::Read>::ReadSamples(
  std::vector<std::vector<std::int32_t>>& data, std::int32_t samplestoread
) {
  // read samples
  samplestoread = std::min(samplestoread, samplesleft);
  std::int32_t bytestoread = samplestoread * blockalign;
  file.read(reinterpret_cast<char*>(filebuffer.data()), bytestoread);
  auto bytesread = static_cast<std::int32_t>(file.gcount());
  std::int32_t samplesread = bytesread / blockalign;

  samplesleft -= samplesread;
  if(samplesread != samplestoread) { std::cerr << "warning: read over eof\n"; }

  MD5::Update(&md5ctx, filebuffer, bytestoread);

  const std::int32_t csize = blockalign / numchannels;
  // decode samples
  if(csize == 1) {
    std::int32_t bufptr = 0;
    for(std::int32_t i = 0; i < samplesread; i++) { // unpack samples
      for(std::int32_t k = 0; k < numchannels; k++) {
        std::uint8_t sample = filebuffer[bufptr];
        bufptr += 1;
        data[k][i] = static_cast<std::int32_t>(sample) - 128;
      }
    }
  } else if(csize == 2) {
    std::int32_t bufptr = 0;
    for(std::int32_t i = 0; i < samplesread; i++) { // unpack samples
      for(std::int32_t k = 0; k < numchannels; k++) {
        auto sample = static_cast<std::int16_t>(
          static_cast<std::uint16_t>(filebuffer[bufptr + 1] << 8U)
          | static_cast<std::uint16_t>(filebuffer[bufptr])
        );
        bufptr += 2;
        data[k][i] = static_cast<std::int32_t>(sample);
      }
    }
  } else if(csize == 3) {
    std::int32_t bufptr = 0;
    for(std::int32_t i = 0; i < samplesread; i++) { // unpack samples
      for(std::int32_t k = 0; k < numchannels; k++) {
        std::uint32_t sample = 0;
        sample = static_cast<std::uint32_t>(filebuffer[bufptr + 2]) << 24U;
        sample |= static_cast<std::uint32_t>(filebuffer[bufptr + 1]) << 16U;
        sample |= static_cast<std::uint32_t>(filebuffer[bufptr]) << 8U;
        bufptr += 3;
        data[k][i] = static_cast<std::int32_t>(sample >> 8U);
      }
    }
  } else {
    std::cerr << "error: unknown csize=" << csize << '\n';
  }

  return samplesread;
}

// Write
Wav<AudioFileBase::Mode::Write>::Wav(
  const std::string& fname, AudioFile<AudioFileBase::Mode::Read>& file,
  bool verbose
) try:
  WavBase(verbose),
  AudioFile(fname, file) {
  kbps = (samplerate * numchannels * bitspersample) / 1000;
  auto csize = static_cast<std::int32_t>(
    std::ceil(static_cast<double>(bitspersample) / 8.)
  );
  byterate = samplerate * numchannels * csize;
  blockalign = numchannels * csize;
} catch(AudioFileErr&) { throw; }

void Wav<AudioFileBase::Mode::Write>::WriteHeader() {
  std::array<std::uint8_t, 8> buf{};
  while(chunkpos < myChunks.GetNumChunks()) {
    const Chunks::tChunk& chunk = myChunks.wavchunks[chunkpos++];
    BitUtils::put32LH(std::span<std::uint8_t, 4>(buf.data(), 4), chunk.id);
    BitUtils::put32LH(std::span<std::uint8_t, 4>(&buf[4], 4), chunk.csize);
    file.write(reinterpret_cast<char*>(buf.data()), 8);
    if(chunk.id == 0x61746164) { break; }
    if(verbose) { std::cout << " chunk size " << chunk.data.size() << '\n'; }
    Write(chunk.data, chunk.data.size());
  }
}

std::int32_t Wav<AudioFileBase::Mode::Write>::WriteSamples(
  const std::vector<std::vector<std::int32_t>>& data,
  std::int32_t samplestowrite
) {
  const std::int32_t csize = blockalign / numchannels;
  if(csize == 1) {
    std::int32_t bufptr = 0;
    for(std::int32_t i = 0; i < samplestowrite; i++) { // pack samples
      for(std::int32_t k = 0; k < numchannels; k++) {
        filebuffer[bufptr] = static_cast<std::uint8_t>(
          static_cast<std::uint32_t>(data[k][i] + 128) & 0xffU
        );
        bufptr += 1;
      }
    }
  } else if(csize == 2) {
    std::int32_t bufptr = 0;
    for(std::int32_t i = 0; i < samplestowrite; i++) { // pack samples
      for(std::int32_t k = 0; k < numchannels; k++) {
        auto sample = static_cast<std::int16_t>(data[k][i]);
        filebuffer[bufptr] = static_cast<std::uint16_t>(sample) & 0xffU;
        filebuffer[bufptr + 1] =
          static_cast<std::uint16_t>(static_cast<std::uint16_t>(sample) >> 8U)
          & 0xffU;
        bufptr += 2;
      }
    }
  } else if(csize == 3) {
    std::int32_t bufptr = 0;
    for(std::int32_t i = 0; i < samplestowrite; i++) { // pack samples
      for(std::int32_t k = 0; k < numchannels; k++) {
        std::int32_t sample = data[k][i];
        filebuffer[bufptr] = static_cast<std::uint32_t>(sample) & 0xffU;
        filebuffer[bufptr + 1] =
          (static_cast<std::uint32_t>(sample) >> 8U) & 0xffU;
        filebuffer[bufptr + 2] =
          (static_cast<std::uint32_t>(sample) >> 16U) & 0xffU;
        bufptr += 3;
      }
    }
  }
  std::int32_t bytestowrite = samplestowrite * blockalign;
  file.write(reinterpret_cast<char*>(filebuffer.data()), bytestowrite);

  MD5::Update(&md5ctx, filebuffer, bytestowrite);
  return bytestowrite;
}

// Share
void WavBase::InitFileBuf(std::int32_t maxframesize) {
  filebuffer.resize(static_cast<std::int64_t>(maxframesize) * blockalign);
}
