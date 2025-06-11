#include "shell.h"
#include "standard.h"
#include "../global.h"
#include "../common/timer.h"
#include "../common/utils.h"

#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

Shell::Shell(): mode(ENCODE) {}

void Shell::HandleOptimizeParam(std::string_view val) {
  if(val == "NO" || val == "0") {
    cfg.optimize = 0;
    return;
  }

  std::vector<std::string> vs;
  StrUtils::SplitToken(val, vs, ",");
  if(vs.size() >= 2) {
    cfg.ocfg.fraction = std::clamp(StrUtils::stod_safe(vs[0]), 0., 1.);
    cfg.ocfg.maxnfunc = std::clamp(std::stoi(vs[1]), 0, 50000);
    if(vs.size() >= 3) {
      std::string cf = StrUtils::str_up(vs[2]);
      if(cf == "L1") cfg.ocfg.optimize_cost = FrameCoder::SearchCost::L1;
      else if(cf == "RMS") cfg.ocfg.optimize_cost = FrameCoder::SearchCost::RMS;
      else if(cf == "GLB")
        cfg.ocfg.optimize_cost = FrameCoder::SearchCost::Golomb;
      else if(cf == "ENT")
        cfg.ocfg.optimize_cost = FrameCoder::SearchCost::Entropy;
      else if(cf == "BPN")
        cfg.ocfg.optimize_cost = FrameCoder::SearchCost::Bitplane;
      else std::cerr << "warning: unknown cost function '" << vs[2] << "'\n";
    }
    if(vs.size() >= 4) { cfg.ocfg.optk = std::clamp(stoi(vs[3]), 1, 32); }
    if(cfg.ocfg.fraction > 0. && cfg.ocfg.maxnfunc > 0) cfg.optimize = 1;
    else cfg.optimize = 0;
  } else {
    std::cerr << "unknown option: " << val << '\n';
  }
}

void Shell::HandleOptCfgParam(std::string_view val) {
  std::vector<std::string> vs;
  StrUtils::SplitToken(val, vs, ",");
  if(vs.size() >= 1) {
    std::string searchMethod = StrUtils::str_up(vs[0]);
    if(searchMethod == "DDS")
      cfg.ocfg.optimize_search = FrameCoder::SearchMethod::DDS;
    else if(searchMethod == "DE")
      cfg.ocfg.optimize_search = FrameCoder::SearchMethod::DE;
    else std::cerr << "  warning: invalid opt-cfg='" << searchMethod << "'\n";
  }
  if(vs.size() >= 2)
    cfg.ocfg.num_threads = std::clamp(std::stoi(vs[1]), 0, 256);
  if(vs.size() >= 3) cfg.ocfg.sigma = std::clamp(StrUtils::stod_safe(vs[2]), 0., 1.);
}

std::unordered_map<
  std::string_view, std::function<void(Shell&, std::string_view)>>
Shell::CreateParamHandlers() {
  std::unordered_map<
    std::string_view, std::function<void(Shell&, std::string_view)>>
    handlers;

  // Mode settings
  handlers["--ENCODE"] = [](Shell& s, auto) { s.mode = ENCODE; };
  handlers["--DECODE"] = [](Shell& s, auto) { s.mode = DECODE; };
  handlers["--LIST"] = [](Shell& s, auto) { s.mode = LIST; };
  handlers["--LISTFULL"] = [](Shell& s, auto) { s.mode = LISTFULL; };

  // Verbosity level
  handlers["--VERBOSE"] = [](Shell& s, auto val) {
    if(val.length()) s.cfg.verbose_level = std::max(0, stoi(std::string(val)));
    else s.cfg.verbose_level = 1;
  };

  // Optimization levels
  const auto set_optimization =
    [](Shell& s, double fraction, int maxnfunc, double sigma) {
      s.cfg.optimize = 1;
      s.cfg.ocfg.fraction = fraction;
      s.cfg.ocfg.maxnfunc = maxnfunc;
      s.cfg.ocfg.sigma = sigma;
    };

  handlers["--NORMAL"] = [](Shell& s, auto) { s.cfg.optimize = 0; };
  handlers["--HIGH"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.075, 100, 0.2);
    s.cfg.ocfg.dds_cfg.c_fail_max = 30;
  };
  handlers["--VERYHIGH"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.2, 250, 0.2);
  };
  handlers["--EXTRAHIGH"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.25, 500, 0.2);
  };
  handlers["--BEST"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.5, 1000, 0.2);
    s.cfg.ocfg.optimize_cost = FrameCoder::SearchCost::Bitplane;
  };
  handlers["--INSANE"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.5, 1500, 0.25);
    s.cfg.ocfg.optimize_cost = FrameCoder::SearchCost::Bitplane;
  };
  handlers["--OPTIMIZE"] = [](Shell& s, auto val) {
    s.HandleOptimizeParam(val);
  };

  // Other parameters
  handlers["--FRAMELEN"] = [](Shell& s, auto val) {
    if(val.length()) s.cfg.max_framelen = std::max(0, stoi(std::string(val)));
  };
  handlers["--MT-MODE"] = [](Shell& s, auto val) {
    if(val.length()) s.cfg.mt_mode = std::max(0, stoi(std::string(val)));
  };
  handlers["--SPARSE-PCM"] = [](Shell& s, auto val) {
    if(val == "NO" || val == "0") s.cfg.sparse_pcm = 0;
    else s.cfg.sparse_pcm = 1;
  };
  handlers["--STEREO-MS"] = [](Shell& s, auto) { s.cfg.stereo_ms = 1; };
  handlers["--OPT-RESET"] = [](Shell& s, auto) { s.cfg.ocfg.reset = 1; };
  handlers["--OPT-CFG"] = [](Shell& s, auto val) { s.HandleOptCfgParam(val); };
  handlers["--ADAPT-BLOCK"] = [](Shell& s, auto val) {
    if(val == "NO" || val == "0") s.cfg.adapt_block = 0;
    else s.cfg.adapt_block = 1;
  };
  handlers["--ZERO-MEAN"] = [](Shell& s, auto val) {
    if(val == "NO" || val == "0") s.cfg.zero_mean = 0;
    else s.cfg.zero_mean = 1;
  };

  return handlers;
}


int Shell::Parse(std::span<const char*> args) {
  if(args.size() < 2) {
    std::cout << SACHelp;
    return 1;
  }

  // Initialize parameter handlers
  static const auto param_handlers = CreateParamHandlers();

  bool first = true;
  for(std::string_view param: args | std::views::drop(1)) {
    std::string uparam = StrUtils::str_up(param);
    std::vector<std::string> kv;
    StrUtils::SplitToken(uparam, kv, "=");
    kv.resize(2);
    std::string &key = kv.at(0), val = kv.at(1);

    if(param.starts_with("--")) {
      if(param_handlers.contains(key)) {
        param_handlers.at(key)(*this, val);
      } else {
        std::cerr << "warning: unknown option '" << param << "'\n";
      }
    } else {
      if(first) {
        sinputfile = param;
        first = false;
      } else soutputfile = param;
    }
  }
  // configure opt method
  if(cfg.ocfg.optimize_search == FrameCoder::SearchMethod::DDS) {
    cfg.ocfg.dds_cfg.nfunc_max = cfg.ocfg.maxnfunc;
    cfg.ocfg.dds_cfg.num_threads = cfg.ocfg.num_threads; // also accepts zero
    cfg.ocfg.dds_cfg.sigma_init = cfg.ocfg.sigma;
  } else if(cfg.ocfg.optimize_search == FrameCoder::SearchMethod::DE) {
    cfg.ocfg.de_cfg.nfunc_max = cfg.ocfg.maxnfunc;
    cfg.ocfg.de_cfg.num_threads = std::max(cfg.ocfg.num_threads, 1);
    cfg.ocfg.de_cfg.sigma_init = cfg.ocfg.sigma;
  }

  return 0;
}

int Shell::Process() {
  Timer myTimer;
  myTimer.start();

  switch(mode) {
    case ENCODE:
      if(Standard::ProcessEncode(sinputfile, soutputfile, cfg)) return 1;
      break;
    case DECODE:
      Standard::ProcessDecode(sinputfile, soutputfile, cfg);
      break;
    case LIST:
      Standard::ProcessList(sinputfile, cfg, false);
      break;
    case LISTFULL:
      Standard::ProcessList(sinputfile, cfg, true);
      break;
  }

  myTimer.stop();
  std::cout << "\n  Time:    ["
            << miscUtils::getTimeStrFromSeconds(round(myTimer.elapsedS()))
            << "]" << std::endl;
  return 0;
}

void Shell::SACInfo() {
  const int kValueWidth = 35;

  std::cout << "+----------------------- Sac -----------------------+\n"
            << "| State-of-the-art lossless audio compression model |\n"
            << "+---------------------------------------------------+\n"
            << "| Copyright (c) 2024 Sebastian Lehmann  MIT License |\n"
            << "+---------------------------------------------------+\n"
            << "| Version       " << std::setw(kValueWidth) << std::right
            << SAC_VERSION << " |\n"
            << "| Compiler      " << std::setw(kValueWidth) << std::right
            << COMPILER << " |\n"
            << "| Architecture  " << std::setw(kValueWidth) << std::right
            << ARCHITECTURE << " |\n"
            << "| AVX State     " << std::setw(kValueWidth) << std::right
            << AVX_STATE << " |\n"
            << "+---------------------------------------------------+\n";
}
