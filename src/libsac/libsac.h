#ifndef CODEC_H
#define CODEC_H

#include "../file/sac.h"
#include "../file/wav.h"
#include "../opt/cma.h"
#include "../opt/dds.h"
#include "../opt/de.h"
#include "cost.h"
#include "profile.h"

#include <cstdint>

class FrameCoder {
public:
  enum class SearchCost : std::uint8_t {
    L1,
    RMS,
    Entropy,
    Golomb,
    Bitplane
  };
  enum class SearchMethod : std::uint8_t {
    DDS,
    DE,
    CMA
  };

  using tch_samples = std::vector<std::vector<std::int32_t>>;

  struct toptim_cfg {
    OptDDS::DDSCfg dds_cfg;
    OptDE::DECfg de_cfg;
    OptCMA::CMACfg cma_cfg;
    std::int32_t reset = 0;
    double fraction = 0;
    std::int32_t maxnfunc = 0;
    std::int32_t num_threads = 0;
    double sigma = 0.2;
    std::int32_t optk = 4;
    SearchMethod optimize_search = SearchMethod::DDS;
    SearchCost optimize_cost = SearchCost::Entropy;
  };

  struct tsac_cfg {
    std::int32_t optimize = 0;
    std::int32_t sparse_pcm = 1;
    std::int32_t zero_mean = 1;
    std::int32_t max_framelen = 20;
    std::int32_t verbose_level = 0;
    std::int32_t stereo_ms = 0;
    std::int32_t mt_mode = 2;
    std::int32_t adapt_block = 1;

    toptim_cfg ocfg;
    SacProfile profiledata;
  };

  FrameCoder(
    std::int32_t numchannels, std::int32_t framesize, const tsac_cfg& sac_cfg
  );

  void SetNumSamples(std::int32_t nsamples) { numsamples_ = nsamples; };

  std::int32_t GetNumSamples() const { return numsamples_; };

  void Predict();
  void Unpredict();
  void Encode();
  void Decode();
  void WriteEncoded(AudioFile<AudioFileBase::Mode::Write>& fout);
  void ReadEncoded(AudioFile<AudioFileBase::Mode::Read>& fin);
  std::vector<std::vector<std::int32_t>> samples, error, s2u_error,
    s2u_error_map, pred;
  std::vector<BufIO> encoded, enc_temp1, enc_temp2;
  std::vector<SacProfile::FrameStats> framestats;

  static std::int32_t WriteBlockHeader(
    std::fstream& file, const std::vector<SacProfile::FrameStats>& framestats,
    std::int32_t ch
  );
  static std::int32_t ReadBlockHeader(
    std::fstream& file, std::vector<SacProfile::FrameStats>& framestats,
    std::int32_t ch
  );

private:
  void CnvError_S2U(const tch_samples& error, std::int32_t numsamples);
  void SetParam(
    Predictor::tparam& param, const SacProfile& profile, bool optimize = false
  ) const;
  void PrintProfile(SacProfile& profile);
  static void
  EncodeProfile(const SacProfile& profile, std::vector<std::uint8_t>& buf);
  static void
  DecodeProfile(SacProfile& profile, const std::vector<std::uint8_t>& buf);
  void AnalyseMonoChannel(std::int32_t ch, std::int32_t numsamples);
  double AnalyseStereoChannel(
    std::int32_t ch0, std::int32_t ch1, std::int32_t numsamples
  );
  void ApplyMs(std::int32_t ch0, std::int32_t ch1, std::int32_t numsamples);
  // void InterChannel(std::int32_t ch0,std::int32_t ch1,std::int32_t
  // numsamples);
  std::size_t
  EncodeMonoFrame_Normal(std::int32_t ch, std::int32_t numsamples, BufIO& buf);
  std::size_t
  EncodeMonoFrame_Mapped(std::int32_t ch, std::int32_t numsamples, BufIO& buf);
  void Optimize(
    const FrameCoder::toptim_cfg& ocfg, SacProfile& profile,
    const std::vector<std::int32_t>& params_to_optimize
  );
  double GetCost(
    const std::shared_ptr<CostFunction>& func, const tch_samples& samples,
    std::size_t samples_to_optimize
  ) const;
  void PredictFrame(
    const SacProfile& profile, tch_samples& error, std::int32_t from,
    std::int32_t numsamples, bool optimize
  );
  void UnpredictFrame(const SacProfile& profile, std::int32_t numsamples);
  double CalcRemapError(std::int32_t ch, std::int32_t numsamples);
  void EncodeMonoFrame(std::int32_t ch, std::int32_t numsamples);
  void DecodeMonoFrame(std::int32_t ch, std::int32_t numsamples);
  std::int32_t numchannels_, framesize_, numsamples_;
  std::int32_t profile_size_bytes_;
  SacProfile base_profile;
  tsac_cfg cfg;
};

class Codec {
  enum class ErrorCode : std::uint8_t {
    COULD_NOT_WRITE
  };

  struct tsub_frame {
    std::int32_t state = -1;
    std::int32_t start = 0;
    std::int32_t length = 0;
  };

public:
  Codec() = default;
  explicit Codec(FrameCoder::tsac_cfg& cfg): opt_(cfg) {};
  std::int32_t EncodeFile(
    Wav<AudioFileBase::Mode::Read>& myWav,
    Sac<AudioFileBase::Mode::Write>& mySac
  );
  // void EncodeFile(Wav &myWav,Sac &mySac,std::int32_t profile,std::int32_t
  // optimize,std::int32_t sparse_pcm);
  void DecodeFile(
    Sac<AudioFileBase::Mode::Read>& mySac,
    Wav<AudioFileBase::Mode::Write>& myWav
  );
  static void ScanFrames(Sac<AudioFileBase::Mode::Read>& mySac);

private:
  std::vector<Codec::tsub_frame> Analyse(
    const std::vector<std::vector<std::int32_t>>& samples,
    std::int32_t blocksamples, std::int32_t min_frame_length,
    std::int32_t samples_read
  );
  void PushState(
    std::vector<Codec::tsub_frame>& sub_frames, Codec::tsub_frame& curframe,
    std::int32_t min_frame_length, std::int32_t block_state,
    std::int32_t samples_block
  ) const;
  static std::pair<double, double>
  AnalyseSparse(std::span<const std::int32_t> buf);
  static void
  PrintProgress(std::int32_t samplesprocessed, std::int32_t totalsamples);
  FrameCoder::tsac_cfg opt_;
  // std::int32_t framesize;
};

#endif
