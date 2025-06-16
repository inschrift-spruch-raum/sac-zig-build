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

  enum class Err {
    OpenReadFail,
    OpenWriteFail,

    IllegalWaw,
    IllegalSac,
    IllegalFormat,
  };

  std::string CostStr(const FrameCoder::SearchCost cost_func);
  std::string SearchStr(const FrameCoder::SearchMethod search_func);
  void PrintAudioInfo(const AudioFile& file);
  void PrintMode(FrameCoder::tsac_cfg& cfg);
  std::expected<void, Err> Encode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  );
  std::expected<void, Lib::Err> Decode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  );
  std::expected<void, Lib::Err> List(
    const std::string& input, FrameCoder::tsac_cfg& config, bool full
  );
} // namespace Lib
