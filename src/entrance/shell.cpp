#include "shell.h"

#include "../common/timer.h"
#include "../common/utils.h"
#include "../file/sac.h"
#include "../global.h"

#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

Shell::Shell(): mode(ENCODE) {}

void Shell::PrintWav(const AudioFile& myWav) {
  std::cout << "  WAVE  Codec: PCM (" << myWav.getKBPS() << " kbps)\n"
            << "  " << myWav.getSampleRate() << "Hz "
            << myWav.getBitsPerSample() << " Bit  ";
  if(myWav.getNumChannels() == 1) std::cout << "Mono";
  else if(myWav.getNumChannels() == 2) std::cout << "Stereo";
  else std::cout << myWav.getNumChannels() << " Channels";
  std::cout << "\n"
            << "  " << myWav.getNumSamples() << " Samples ["
            << miscUtils::getTimeStrFromSamples(
                 myWav.getNumSamples(), myWav.getSampleRate()
               )
            << "]\n";
}

std::string Shell::CostStr(const FrameCoder::SearchCost cost_func) {
  using enum FrameCoder::SearchCost;
  std::string rstr;
  switch(cost_func) {
    case L1: rstr = "L1"; break;
    case RMS: rstr = "rms"; break;
    case Golomb: rstr = "glb"; break;
    case Entropy: rstr = "ent"; break;
    case Bitplane: rstr = "bpn"; break;
    default: break;
  }
  return rstr;
}

std::string Shell::SearchStr(const FrameCoder::SearchMethod search_func) {
  using enum FrameCoder::SearchMethod;
  std::string rstr;
  switch(search_func) {
    case DDS: rstr = "DDS"; break;
    case DE: rstr = "DE"; break;
    default: break;
  }
  return rstr;
}

void Shell::PrintMode() {
  const FrameCoder::toptim_cfg& ocfg = opt.ocfg;
  std::cout << "  Profile: "
            << "mt" << opt.mt_mode << " " << opt.max_framelen << "s";
  if(opt.adapt_block) std::cout << " ab";
  if(opt.zero_mean) std::cout << " zero-mean";
  if(opt.sparse_pcm) std::cout << " sparse-pcm";
  std::cout << '\n';
  if(opt.optimize) {
    std::cout << "  Optimize: " << SearchStr(ocfg.optimize_search) << " "
              << std::format("{:.1f}%", ocfg.fraction * 100.0)
              << ",n=" << ocfg.maxnfunc << "," << CostStr(ocfg.optimize_cost)
              << ",k=" << ocfg.optk << '\n';
  }
  std::cout << std::endl;
}

void Shell::HandleOptimizeParam(std::string_view val) {
  if(val == "NO" || val == "0") {
    opt.optimize = 0;
    return;
  }

  std::vector<std::string> vs;
  StrUtils::SplitToken(val, vs, ",");
  if(vs.size() >= 2) {
    opt.ocfg.fraction = std::clamp(stod_safe(vs[0]), 0., 1.);
    opt.ocfg.maxnfunc = std::clamp(std::stoi(vs[1]), 0, 50000);
    if(vs.size() >= 3) {
      std::string cf = StrUtils::str_up(vs[2]);
      if(cf == "L1") opt.ocfg.optimize_cost = FrameCoder::SearchCost::L1;
      else if(cf == "RMS") opt.ocfg.optimize_cost = FrameCoder::SearchCost::RMS;
      else if(cf == "GLB")
        opt.ocfg.optimize_cost = FrameCoder::SearchCost::Golomb;
      else if(cf == "ENT")
        opt.ocfg.optimize_cost = FrameCoder::SearchCost::Entropy;
      else if(cf == "BPN")
        opt.ocfg.optimize_cost = FrameCoder::SearchCost::Bitplane;
      else std::cerr << "warning: unknown cost function '" << vs[2] << "'\n";
    }
    if(vs.size() >= 4) { opt.ocfg.optk = std::clamp(stoi(vs[3]), 1, 32); }
    if(opt.ocfg.fraction > 0. && opt.ocfg.maxnfunc > 0) opt.optimize = 1;
    else opt.optimize = 0;
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
      opt.ocfg.optimize_search = FrameCoder::SearchMethod::DDS;
    else if(searchMethod == "DE")
      opt.ocfg.optimize_search = FrameCoder::SearchMethod::DE;
    else std::cerr << "  warning: invalid opt-cfg='" << searchMethod << "'\n";
  }
  if(vs.size() >= 2)
    opt.ocfg.num_threads = std::clamp(std::stoi(vs[1]), 0, 256);
  if(vs.size() >= 3) opt.ocfg.sigma = std::clamp(stod_safe(vs[2]), 0., 1.);
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
    if(val.length()) s.opt.verbose_level = std::max(0, stoi(std::string(val)));
    else s.opt.verbose_level = 1;
  };

  // Optimization levels
  const auto set_optimization =
    [](Shell& s, double fraction, int maxnfunc, double sigma) {
      s.opt.optimize = 1;
      s.opt.ocfg.fraction = fraction;
      s.opt.ocfg.maxnfunc = maxnfunc;
      s.opt.ocfg.sigma = sigma;
    };

  handlers["--NORMAL"] = [](Shell& s, auto) { s.opt.optimize = 0; };
  handlers["--HIGH"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.075, 100, 0.2);
    s.opt.ocfg.dds_cfg.c_fail_max = 30;
  };
  handlers["--VERYHIGH"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.2, 250, 0.2);
  };
  handlers["--EXTRAHIGH"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.25, 500, 0.2);
  };
  handlers["--BEST"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.5, 1000, 0.2);
    s.opt.ocfg.optimize_cost = FrameCoder::SearchCost::Bitplane;
  };
  handlers["--INSANE"] = [set_optimization](Shell& s, auto) {
    set_optimization(s, 0.5, 1500, 0.25);
    s.opt.ocfg.optimize_cost = FrameCoder::SearchCost::Bitplane;
  };
  handlers["--OPTIMIZE"] = [](Shell& s, auto val) {
    s.HandleOptimizeParam(val);
  };

  // Other parameters
  handlers["--FRAMELEN"] = [](Shell& s, auto val) {
    if(val.length()) s.opt.max_framelen = std::max(0, stoi(std::string(val)));
  };
  handlers["--MT-MODE"] = [](Shell& s, auto val) {
    if(val.length()) s.opt.mt_mode = std::max(0, stoi(std::string(val)));
  };
  handlers["--SPARSE-PCM"] = [](Shell& s, auto val) {
    if(val == "NO" || val == "0") s.opt.sparse_pcm = 0;
    else s.opt.sparse_pcm = 1;
  };
  handlers["--STEREO-MS"] = [](Shell& s, auto) { s.opt.stereo_ms = 1; };
  handlers["--OPT-RESET"] = [](Shell& s, auto) { s.opt.ocfg.reset = 1; };
  handlers["--OPT-CFG"] = [](Shell& s, auto val) { s.HandleOptCfgParam(val); };
  handlers["--ADAPT-BLOCK"] = [](Shell& s, auto val) {
    if(val == "NO" || val == "0") s.opt.adapt_block = 0;
    else s.opt.adapt_block = 1;
  };
  handlers["--ZERO-MEAN"] = [](Shell& s, auto val) {
    if(val == "NO" || val == "0") s.opt.zero_mean = 0;
    else s.opt.zero_mean = 1;
  };

  return handlers;
}

double Shell::stod_safe(const std::string& str) {
  double d;
  try {
    d = std::stod(str);
  } catch(const std::invalid_argument&) {
    std::cerr << "stod: argument is invalid\n";
    throw;
  } catch(const std::out_of_range&) {
    std::cerr << "stod: argument is out of range for a double\n";
    throw;
  }
  return d;
}

int Shell::Parse(std::span<char*> args) {
  if(args.size() < 2) {
    std::cout << SACHelp;
    return 1;
  }

  // Initialize parameter handlers
  static const auto param_handlers = CreateParamHandlers();

  bool first = true;
  for(std::string_view param: args) {
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
  if(opt.ocfg.optimize_search == FrameCoder::SearchMethod::DDS) {
    opt.ocfg.dds_cfg.nfunc_max = opt.ocfg.maxnfunc;
    opt.ocfg.dds_cfg.num_threads = opt.ocfg.num_threads; // also accepts zero
    opt.ocfg.dds_cfg.sigma_init = opt.ocfg.sigma;
  } else if(opt.ocfg.optimize_search == FrameCoder::SearchMethod::DE) {
    opt.ocfg.de_cfg.nfunc_max = opt.ocfg.maxnfunc;
    opt.ocfg.de_cfg.num_threads = std::max(opt.ocfg.num_threads, 1);
    opt.ocfg.de_cfg.sigma_init = opt.ocfg.sigma;
  }

  return 0;
}

int Shell::Process() {
  Timer myTimer;
  myTimer.start();

  if(mode == ENCODE) {
    Wav myWav(opt.verbose_level > 0);
    std::cout << "Open: '" << sinputfile << "': ";
    if(myWav.OpenRead(sinputfile) == 0) {
      std::cout << "ok (" << myWav.getFileSize() << " Bytes)\n";
      if(myWav.ReadHeader() == 0) {
        PrintWav(myWav);

        bool fsupp =
          (myWav.getBitsPerSample() <= 24)
          && ((myWav.getNumChannels() == 2) || (myWav.getNumChannels() == 1));
        if(!fsupp) {
          std::cerr << "unsupported input format" << std::endl
                    << "must be 1-16 bit, mono/stereo, pcm" << std::endl;
          myWav.Close();
          return 1;
        }
        Sac mySac(myWav);
        std::cout << "Create: '" << soutputfile << "': ";
        if(mySac.OpenWrite(soutputfile) == 0) {
          std::cout << "ok\n";
          PrintMode();
          Codec myCodec(opt);

          Timer time;
          time.start();
          myCodec.EncodeFile(myWav, mySac);
          time.stop();

          uint64_t infilesize = myWav.getFileSize();
          uint64_t outfilesize = mySac.readFileSize();
          double r = 0., bps = 0.;
          if(outfilesize) {
            r = outfilesize * 100.0 / infilesize;
            bps = (outfilesize * 8.)
                  / static_cast<double>(
                    myWav.getNumSamples() * myWav.getNumChannels()
                  );
          }
          double xrate = 0.0;
          if(time.elapsedS() > 0.0)
            xrate = (myWav.getNumSamples() / double(myWav.getSampleRate()))
                    / time.elapsedS();

          std::cout << "\n  " << infilesize << "->" << outfilesize << "="
                    << std::format("{:.1f}", r) << "% ("
                    << std::format("{:.3f}", bps) << " bps)"
                    << "  " << std::format("{:.3f}x", xrate) << '\n';
          mySac.Close();
        } else std::cout << "could not create\n";
      } else std::cout << "warning: input is not a valid .wav file\n";
      myWav.Close();
    } else std::cout << "could not open\n";
  } else if(mode == LIST || mode == LISTFULL || mode == DECODE) {
    Sac mySac;
    std::cout << "Open: '" << sinputfile << "': ";
    if(mySac.OpenRead(sinputfile) == 0) {
      std::streampos FileSizeSAC = mySac.getFileSize();
      std::cout << "ok (" << FileSizeSAC << " Bytes)\n";
      if(mySac.ReadSACHeader() == 0) {
        uint8_t md5digest[16];
        mySac.ReadMD5(md5digest);
        double bps =
          (static_cast<double>(FileSizeSAC) * 8.0)
          / static_cast<double>(mySac.getNumSamples() * mySac.getNumChannels());
        int kbps =
          round((mySac.getSampleRate() * mySac.getNumChannels() * bps) / 1000);
        mySac.setKBPS(kbps);
        PrintWav(mySac);
        std::cout << "  Profile: "
                  << "mt" << opt.mt_mode << " "
                  << static_cast<int>(mySac.mcfg.max_framelen) << "s\n"
                  << "  Ratio:   " << std::fixed << std::setprecision(3) << bps
                  << " bps\n\n"
                  << "  Audio MD5: ";
        for(auto x: md5digest) std::cout << std::hex << (int)x;
        std::cout << std::dec << '\n';

        if(mode == LISTFULL) {
          Codec myCodec;
          myCodec.ScanFrames(mySac);
        } else if(mode == DECODE) {
          Wav myWav(mySac);
          std::cout << "Create: '" << soutputfile << "': ";
          if(myWav.OpenWrite(soutputfile) == 0) {
            std::cout << "ok\n";

            Timer time;
            time.start();

            Codec myCodec(opt);
            myCodec.DecodeFile(mySac, myWav);
            MD5::Finalize(&myWav.md5ctx);
            time.stop();

            double xrate = 0.0;
            if(time.elapsedS() > 0.0)
              xrate = (myWav.getNumSamples() / double(myWav.getSampleRate()))
                      / time.elapsedS();
            std::cout << "\n  Speed " << std::format("{:.3f}x", xrate) << '\n';

            std::cout << "  Audio MD5: ";
            bool md5diff = std::memcmp(myWav.md5ctx.digest, md5digest, 16);
            if(!md5diff) std::cout << "ok\n";
            else {
              std::cout << "Error (";
              for(auto x: myWav.md5ctx.digest) std::cout << std::hex << (int)x;
              std::cout << std::dec << ")\n";
            }
            myWav.Close();
          } else std::cout << "could not create\n";
        }
      } else std::cout << "warning: input is not a valid .sac file\n";
    }
  } else std::cout << "could not open\n";

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
