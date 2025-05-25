#pragma once
#include "../libsac/libsac.h"

namespace Standard {
  std::string CostStr(const FrameCoder::SearchCost cost_func);
  std::string SearchStr(const FrameCoder::SearchMethod search_func);
  void PrintAudioInfo(const AudioFile& file);
  void PrintMode(FrameCoder::coder_ctx& opt);
  int ProcessEncode(
    const std::string& input, const std::string& output,
    FrameCoder::coder_ctx& config
  );
  void ProcessDecode(
    const std::string& input, const std::string& output,
    FrameCoder::coder_ctx& config
  );
  void ProcessList(
    const std::string& input, FrameCoder::coder_ctx& config, bool full
  );
} // namespace Standard
