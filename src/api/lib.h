#pragma once
#include "../libsac/libsac.h"

namespace Standard {
  std::string CostStr(const FrameCoder::SearchCost cost_func);
  std::string SearchStr(const FrameCoder::SearchMethod search_func);
  void PrintAudioInfo(const AudioFile& file);
  void PrintMode(FrameCoder::tsac_cfg& cfg);
  int ProcessEncode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  );
  void ProcessDecode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  );
  void ProcessList(
    const std::string& input, FrameCoder::tsac_cfg& config, bool full
  );
} // namespace Standard
