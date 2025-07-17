#pragma once // UTILS_H

#include "../global.h"
#include "math.h"

#include <algorithm>
#include <numeric>
#include <cstring>
#include <cmath>
#include <immintrin.h>
#include <string_view>
#include <unordered_map>

// running exponential smoothing
// sum=alpha*sum+(1.0-alpha)*val, where 1/(1-alpha) is the mean number of samples considered
class RunExp {
  public:
      RunExp(double alpha):sum(0.0),alpha(alpha){};
      RunExp(double alpha,double sum):sum(sum),alpha(alpha){};
      inline void Update(double val) {
        sum=alpha*sum+(1.-alpha)*val;
      }
    double sum;
  private:
    double alpha;
};

// running weighted sum: sum_{i=0}^n alpha^(n-i) val
class RunWeight {
  public:
      RunWeight(double alpha):sum(0.0),alpha(alpha){};
      inline void Update(double val) {
        sum=alpha*sum+val;
      }
    double sum;
  private:
    double alpha;
};

template <std::int32_t tEMA=1,std::int32_t tbias_corr=0>
class RunSum {
  public:
    RunSum(double alpha)
    :alpha(alpha),power_alpha(1.0),sum(0.0)
    {
    }
    inline void Update(double val)
    {
      if constexpr(tEMA)
        sum = alpha*sum + (1.0-alpha)*val;
      else
        sum = alpha*sum + val;

      if constexpr(tbias_corr)
        power_alpha*=alpha;
    }
    inline double Get() const
    {
      if constexpr(tbias_corr)
      {
        const double denom=1.0-power_alpha;
        assert(denom>0); // must call Update() before first Get()
        return sum/denom;
      } else
        return sum;
    }
  protected:
    double alpha,power_alpha,sum;
};

using RunSumEMA = RunSum<1,0>;
using RunSumEMA_BC = RunSum<1,1>;
using RunSumGEO = RunSum<0,0>;
using RunSumGEO_BC = RunSum<0,1>;

class RunMeanVar {
  public:
    RunMeanVar(double alpha)
    :alpha_(alpha),first_(true)
    {
      mean_ = var_ = 0.0;
    }
    void Update(double val)
    {
      /*if (first_) {
        mean_ = val;
        var_ = 0.0;
        first_ = false;
      } else*/
      {
        if constexpr(0) {
          double delta = val - mean_;
          mean_ += (1 - alpha_) * delta;
          var_ = alpha_ * var_ + (1 - alpha_) * delta * delta;
        } else {// slightly more stable
          // Welford
          double old_mean = mean_;
          mean_=alpha_*mean_+(1.0-alpha_)*val;
          var_=alpha_*var_+(1.0-alpha_)*((val-old_mean)*(val-mean_));
        }
      }
    }
    auto get()
    {
      return std::pair{mean_,std::max(0.0,var_)};
    }
  protected:
    double alpha_;
    bool first_;
    double mean_,var_;
};

namespace StrUtils {
      inline void StrUpper(std::string &str)
      {
        std::transform(str.begin(), str.end(),str.begin(), ::toupper);
      }
      inline std::string str_up(const std::string_view str)
      {
        std::string ts;
        for (auto c:str) ts += toupper(c);
        return ts;
      }
      inline void SplitToken(const std::string_view str,std::vector<std::string>& tokens,const std::string_view delimiters)
      {
        auto lastPos = str.find_first_not_of(delimiters, 0); // Skip delimiters at beginning.
        auto pos     = str.find_first_of(delimiters, lastPos); // Find first "non-delimiter".

        while (std::string::npos != pos || std::string::npos != lastPos)  {
          tokens.push_back(std::string(str.substr(lastPos, pos - lastPos))); // Found a token, add it to the vector.
          lastPos = str.find_first_not_of(delimiters, pos); // Skip delimiters.  Note the "not_of"
          pos = str.find_first_of(delimiters, lastPos); // Find next "non-delimiter"
        }
      }
      inline void RemoveWhite(std::string &str,const std::string &whites)
      {
        auto firstPos = str.find_first_not_of(whites);

        if (firstPos!=std::string::npos) {
          auto lastPos = str.find_last_not_of(whites);
          str=str.substr(firstPos,lastPos-firstPos+1);
        } else str="";
      }
      inline void SplitFloat(const std::string &str,std::vector<float>&x)
      {
        std::vector <std::string> tokens;
        SplitToken(str,tokens,",");
        for (auto &token:tokens) x.push_back(std::stof(token));
      }

      inline double stod_safe(const std::string& str) {
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

      template<typename E>
      inline std::string EnumToStr(E value, const std::unordered_map<E, std::string_view>& mapping) {
        if(auto it = mapping.find(value); it != mapping.end()) {
          return std::string(it->second);
        }
        return "";
      }
};

namespace miscUtils {

  enum class MapMode {rec,exp,tanh,power,sigmoid};

  template <MapMode mode>
  double decay_map(double gamma, double val)
  {
    if constexpr (mode == MapMode::rec) return 1.0 / (1.0 + gamma * val);
    else if constexpr (mode == MapMode::exp) return std::exp(-gamma * val);
    else if constexpr (mode == MapMode::tanh) return 1.0-std::tanh(gamma * val);
    else if constexpr (mode == MapMode::power) return std::pow(gamma, val);
    else if constexpr (mode == MapMode::sigmoid) return 1.0 / (1.0 + std::exp(gamma*(val-1.0)));
    return 0;
  }

/*static float rsqrt(float __x)
{
    float reciprocal;
    __asm__ __volatile__ (
        "movss %1, %%xmm0\n"
        "rsqrtss %%xmm0, %%xmm1\n"
        "movss %%xmm1, %0\n"
        :"=m"(reciprocal)
        :"m"(__x)
        :"xmm0", "xmm1"
    );
  return reciprocal;
}*/
  template <typename T>
  void swap_erase(std::vector<T>& e, std::size_t idx)
  {
    if (idx < e.size()) {
      std::swap(e[idx], e.back());
      e.pop_back();
    }
  }

  inline void RollBack(vec1D &data,double input)
  {
    if (data.size()) {
      std::memmove(&data[1],&data[0],(data.size()-1)*sizeof(double));
      data[0]=input;
    }
  }
  inline std::string getTimeStrFromSamples(std::int64_t numsamples,std::int64_t samplerate)
  {
   std::ostringstream ss;
   std::int32_t h,m,s,ms;
   h=m=s=ms=0;
   if (numsamples>0 && samplerate>0) {
     while (numsamples >= 3600*samplerate) {++h;numsamples-=3600*samplerate;};
     while (numsamples >= 60*samplerate) {++m;numsamples-=60*samplerate;};
     while (numsamples >= samplerate) {++s;numsamples-=samplerate;};
     ms=std::round((numsamples*1000.)/samplerate);
   }
   ss << std::setfill('0') << std::setw(2) << h << ":" << std::setw(2) << m << ":" << std::setw(2) << s << "." << ms;
   return ss.str();
 }
 inline std::string getTimeStrFromSeconds(std::int32_t seconds)
 {
   std::ostringstream ss;
   std::int32_t h,m,s;
   h=m=s=0;
   if (seconds>0) {
      while (seconds >= 3600) {++h;seconds-=3600;};
      while (seconds >= 60) {++m;seconds-=60;};
      s=seconds;
   }
   ss << std::setfill('0') << std::setw(2) << h << ":" << std::setw(2) << m << ":" << std::setw(2) << s;
   return ss.str();
 }
 inline std::string ConvertFixed(double val,std::int32_t digits)
 {
   std::ostringstream ss;
   ss << std::fixed << std::setprecision(digits) << val;
   return ss.str();
 }
};

namespace BitUtils {
  std::uint32_t get32HL(std::span<const std::uint8_t, 4> buf);
  std::uint32_t get32LH(std::span<const std::uint8_t, 4> buf);
  std::uint16_t get16LH(std::span<const std::uint8_t, 2> buf);
  void put16LH(std::span<std::uint8_t, 2> buf,std::uint16_t val);
  void put32LH(std::span<std::uint8_t, 4> buf,std::uint32_t val);
  std::string U322Str(std::uint32_t val);
} // namespace BitUtils
