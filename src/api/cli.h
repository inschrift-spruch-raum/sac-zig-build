#pragma once // Shell_H

#include "../libsac/libsac.h"
#include "lib.h"

#include <span>
#include <string>
#include <string_view>

constexpr std::string_view SACHelp =
  "usage: sac [--options] input output\n\n"
  "  --encode            encode input.wav to output.sac (def)\n"
  "    --normal|high|veryhigh|extrahigh compression (def=normal)\n"
  "    --best            you asked for it\n\n"
  "  --decode            decode input.sac to output.wav\n"
  "  --list              list info about input.sac\n"
  "  --listfull          verbose info about input\n"
  "  --verbose           verbose output\n\n"
  "  supported types: 1-16 bit, mono/stereo pcm\n"
  "  advanced options    (automatically set)\n"
  "   --optimize=#       frame-based optimization\n"
  "     no|s,n,c,k       s=[0,1.0],n=[0,10000]\n"
  "                      c=[l1,rms,glb,ent,bpn] k=[1,32]\n"
  "   --opt-cfg=#        configure optimization method\n"
  "     de|dds,nt,s      nt=num threads,s=search radius (def=0.2)\n"
  "   --opt-reset        reset opt params at frame boundaries\n"
  "   --mt-mode=n        multi-threading level n=[0-2]\n"
  "   --zero-mean        zero-mean input\n"
  "   --adapt-block      adaptive frame splitting\n"
  "   --framelen=n       def=20 seconds\n"
  "   --sparse-pcm       enable pcm modelling\n";

class Shell {
public:
  Shell() = default;

  std::int32_t Parse(std::span<const char*> args);
  std::int32_t Process();
  static void SACInfo();

private:
  void HandleOptimizeParam(std::string_view val);
  void HandleOptCfgParam(std::string_view val);
  static std::unordered_map<
    std::string_view, std::function<void(Shell&, std::string_view)>>
  CreateParamHandlers();
  std::string sinputfile, soutputfile;
  Lib::Mode mode{Lib::Mode::ENCODE};
  FrameCoder::tsac_cfg cfg;
};
