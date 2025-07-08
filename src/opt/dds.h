#ifndef DDS_H
#define DDS_H

#include <future>
#include <cassert>
#include "opt.h"

// Dynamical dimensioned search algorithm for computationally efficient watershed model calibration
// Tolson, Shoemaker 2007
class OptDDS : public Opt {
  public:
    struct DDSCfg
    {
      double sigma_init=0.2;
      std::int32_t c_succ_max=3;
      std::int32_t c_fail_max=50;
      std::int32_t num_threads=1;
      std::int32_t nfunc_max=0;
    };
    OptDDS(const DDSCfg &cfg,const box_const &parambox,bool verbose=false);
    ppoint run(opt_func func,const vec1D &xstart) override;
  protected:
    vec1D generate_candidate(const vec1D &x,std::int32_t nfunc,double sigma);
    ppoint run_single(opt_func func,const vec1D &xstart);
    ppoint run_mt(opt_func func,const vec1D &xstart);
    const DDSCfg &cfg;
    bool verbose;
};

#endif // DDS_H

