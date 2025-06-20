#pragma once
#include "../libsac/libsac.h"

#include <expected>

namespace Lib {

  enum class Mode {
    ENCODE,
    DECODE,
    LIST,
    LISTFULL
  };

  std::string CostStr(const FrameCoder::SearchCost cost_func);
  std::string SearchStr(const FrameCoder::SearchMethod search_func);
  template <AudioFileBase::Mode AudioMode>
  void PrintAudioInfo(const AudioFile<AudioMode>& file);
  void PrintMode(FrameCoder::tsac_cfg& cfg);
  std::expected<void, AudioFileBase::Err> Encode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  );
  std::expected<void, AudioFileBase::Err> Decode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  );
  std::expected<void, AudioFileBase::Err> List(
    const std::string& input, FrameCoder::tsac_cfg& config, bool full
  );
} // namespace Lib
