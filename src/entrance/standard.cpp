#include "standard.h"

#include "../common/timer.h"
#include "../common/utils.h"

#include <cstring>
#include <iostream>

namespace Standard {
  std::string CostStr(const FrameCoder::SearchCost cost_func) {
    using enum FrameCoder::SearchCost;
    static const std::unordered_map<FrameCoder::SearchCost, std::string_view>
      cost_map = {
        {      L1,  "L1"},
        {     RMS, "rms"},
        {  Golomb, "glb"},
        { Entropy, "ent"},
        {Bitplane, "bpn"}
    };
    return StrUtils::EnumToStr(cost_func, cost_map);
  }

  std::string SearchStr(const FrameCoder::SearchMethod search_func) {
    using enum FrameCoder::SearchMethod;
    static const std::unordered_map<FrameCoder::SearchMethod, std::string_view>
      search_map = {
        {DDS, "DDS"},
        { DE,  "DE"}
    };
    return StrUtils::EnumToStr(search_func, search_map);
  }

  void PrintAudioInfo(const AudioFile& file) {
    std::cout << "  WAVE  Codec: PCM (" << file.getKBPS() << " kbps)\n"
              << "  " << file.getSampleRate() << "Hz "
              << file.getBitsPerSample() << " Bit  ";
    if(file.getNumChannels() == 1) std::cout << "Mono";
    else if(file.getNumChannels() == 2) std::cout << "Stereo";
    else std::cout << file.getNumChannels() << " Channels";
    std::cout << "\n"
              << "  " << file.getNumSamples() << " Samples ["
              << miscUtils::getTimeStrFromSamples(
                   file.getNumSamples(), file.getSampleRate()
                 )
              << "]\n";
  }

  void PrintMode(FrameCoder::tsac_cfg& cfg) {
    const FrameCoder::toptim_cfg& ocfg = cfg.ocfg;
    std::cout << "  Profile: "
              << "mt" << cfg.mt_mode << " " << cfg.max_framelen << "s";
    if(cfg.adapt_block) std::cout << " ab";
    if(cfg.zero_mean) std::cout << " zero-mean";
    if(cfg.sparse_pcm) std::cout << " sparse-pcm";
    std::cout << '\n';
    if(cfg.optimize) {
      std::cout << "  Optimize: " << SearchStr(ocfg.optimize_search) << " "
                << std::format("{:.1f}%", ocfg.fraction * 100.0)
                << ", n=" << ocfg.maxnfunc << "," << CostStr(ocfg.optimize_cost)
                << ", k=" << ocfg.optk << '\n';
    }
    std::cout << std::endl;
  }

  int ProcessEncode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  ) {
    Wav myWav(config.verbose_level > 0);
    std::cout << "Open: '" << input << "': ";
    if(myWav.OpenRead(input) == 0) {
      std::cout << "ok (" << myWav.getFileSize() << " Bytes)\n";
      if(myWav.ReadHeader() == 0) {
        PrintAudioInfo(myWav);

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
        std::cout << "Create: '" << output << "': ";
        if(mySac.OpenWrite(output) == 0) {
          std::cout << "ok\n";
          PrintMode(config);
          Codec myCodec(config);
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
    return 0;
  }

  void ProcessDecode(
    const std::string& input, const std::string& output,
    FrameCoder::tsac_cfg& config
  ) {
    Sac mySac;
    std::cout << "Open: '" << input << "': ";
    if(mySac.OpenRead(input) == 0) {
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
        PrintAudioInfo(mySac);
        std::cout << "  Profile: "
                  << "mt" << config.mt_mode << " "
                  << static_cast<int>(mySac.mcfg.max_framelen) << "s\n"
                  << "  Ratio:   " << std::fixed << std::setprecision(3) << bps
                  << " bps\n\n"
                  << "  Audio MD5: ";
        for(auto x: md5digest) std::cout << std::hex << (int)x;
        std::cout << std::dec << '\n';

        Wav myWav(mySac);
        std::cout << "Create: '" << output << "': ";
        if(myWav.OpenWrite(output) == 0) {
          std::cout << "ok\n";

          Timer time;
          time.start();
          Codec myCodec(config);
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
      } else std::cout << "warning: input is not a valid .sac file\n";
    }
  }

  void ProcessList(
    const std::string& input, FrameCoder::tsac_cfg& config, bool full
  ) {
    Sac mySac;
    std::cout << "Open: '" << input << "': ";
    if(mySac.OpenRead(input) == 0) {
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
        PrintAudioInfo(mySac);
        std::cout << "  Profile: "
                  << "mt" << config.mt_mode << " "
                  << static_cast<int>(mySac.mcfg.max_framelen) << "s\n"
                  << "  Ratio:   " << std::fixed << std::setprecision(3) << bps
                  << " bps\n\n"
                  << "  Audio MD5: ";
        for(auto x: md5digest) std::cout << std::hex << (int)x;
        std::cout << std::dec << '\n';

        if(full) {
          Codec myCodec;
          myCodec.ScanFrames(mySac);
        }
      } else std::cout << "warning: input is not a valid .sac file\n";
    }
  }

} // namespace Standard
