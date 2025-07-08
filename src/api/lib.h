#pragma once
#include "../libsac/libsac.h"

#include <cstdint>
#include <expected>

namespace Lib {

  enum class Mode : std::uint8_t {
    ENCODE,
    DECODE,
    LIST,
    LISTFULL
  };

  std::string CostStr(FrameCoder::SearchCost cost_func);
  std::string SearchStr(FrameCoder::SearchMethod search_func);
  template<AudioFileBase::Mode AudioMode>
  void PrintAudioInfo(const AudioFile<AudioMode>& file);
  void PrintMode(FrameCoder::tsac_cfg& cfg);
  std::expected<void, AudioFileErr::Err> Encode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  );
  std::expected<void, AudioFileErr::Err> Decode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  );
  std::expected<void, AudioFileErr::Err>
  List(const std::string& input, FrameCoder::tsac_cfg& config, bool full);
} // namespace Lib
