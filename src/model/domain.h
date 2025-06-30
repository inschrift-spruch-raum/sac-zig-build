#pragma once

#include "./model.h"
#include <cmath>
#include <cstdio>

static class LogDomain {
  public:
    int min,max;
    const int scale,dbits,dscale,dmin,dmax;
    LogDomain():scale(256),dbits(12),dscale(1<<dbits),dmin(-(dscale>>1)),dmax((dscale>>1)-1)
    {
      for (int i=0;i<PSCALE;i++)
      {
        FwdTbl[i]=std::floor(std::log((i+0.5)/(PSCALE-i-0.5))*double(scale)+0.5);
      };
      min=FwdTbl[0];
      max=FwdTbl[PSCALE-1];
      //printf("%i %i\n",min,max);
      // 12-Bit
      InvTbl=new int[dscale];
      for (int i=dmin;i<=dmax;i++)
      {
         double p=double(PSCALE)/(1.0+std::exp(-double(i)/double(scale)));
         InvTbl[i-dmin]=std::floor(p);
      };
    }
    ~LogDomain()
    {
      delete []InvTbl;
    }
    inline int Fwd(int p)
    {
       return FwdTbl[p];
    }
    inline int Inv(int x)
    {
       if (x<dmin) return 0;
       else if (x>dmax) return PSCALE-1;
       else return InvTbl[x-dmin];
    }
    void Check()
    {
      int sum=0;
      std::printf("%i %i\n",min,max);
      std::printf("%i  [%i %i]\n",dscale,dmin,dmax);
      std::printf("%i %i\n",Inv(0),Fwd(PSCALEh));
      for (int i=0;i<PSCALE;i++)
      {
        int p=Inv(Fwd(i));
        sum+=(p-i)*(p-i);
      }
      std::printf(" mse: %0.2f\n",double(sum)/double(PSCALE));
    }
  protected:
    int FwdTbl[PSCALE];
    int *InvTbl;
} myDomain;
