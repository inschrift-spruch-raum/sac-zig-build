#ifndef SSE_H
#define SSE_H

#include "counter.h"
#include "domain.h"

/*
 SSE: functions of context history
 maps a probability via (linear-)quantization to a new probability
*/
template <std::int32_t NB>
class SSE {
  std::uint16_t p_quant,px;
  public:
    enum {mapsize=1<<NB};
    SSE ()
    {
      for (std::int32_t i=0;i<=mapsize;i++) // init prob-map that SSE.p1(p)~p
      {
        std::int32_t v=((i*PSCALE)>>NB);
        v = std::clamp(v,1,PSCALEm);
        Map[i].p1=v;
      }
    }
    std::int32_t Predict(std::int32_t p1) // linear interpolate beetween bins
    {
      p_quant=p1>>(PBITS-NB);
      std::int32_t p_mod=p1&(mapsize-1); //std::int32_t p_mod=p1%map_size;
      std::int32_t pl=Map[p_quant].p1;
      std::int32_t ph=Map[p_quant+1].p1;
      px=pl+((p_mod*(ph-pl))>>NB);
      return px;
    }
    void Update(std::int32_t bit,std::int32_t rate) // update both bins
    {
      Map[p_quant].update(bit,rate);
      Map[p_quant+1].update(bit,rate);
    }
    /*void update4(std::int32_t bit,std::int32_t rate) // update four nearest bins
    {
      if (p_quant>0) Map[p_quant-1].update(bit,rate>>1);
      Map[p_quant].update(bit,rate);
      Map[p_quant+1].update(bit,rate);
      if (p_quant<mapsize-1) Map[p_quant+2].update(bit,rate>>1);
    }
    void update1(std::int32_t bit,std::int32_t rate) // update artifical bin
    {
      LinearCounter16 tmp;
      tmp.p1=px;
      tmp.update(bit,rate);
      std::int32_t pm=tmp.p1-px;
      std::int32_t pt1=Map[p_quant].p1+pm;
      std::int32_t pt2=Map[p_quant+1].p1+pm;
      Map[p_quant].p1=clamp(pt1,1,PSCALEm);
      Map[p_quant+1].p1=clamp(pt2,1,PSCALEm);
    }*/
  protected:
    LinearCounter16 Map[(1<<NB)+1];
};

// Maps a state to a probability
class HistProbMapping
{
  enum {NUMSTATES=256};
  public:
    HistProbMapping()
    {
      for (std::int32_t i=0;i<NUMSTATES;i++) Map[i].p1=StateProb::GetP1(i);
    };
    inline std::int32_t p1(std::uint8_t state)
    {
       st=state;
       return Map[state].p1;
    }
    void Update(std::int32_t bit,std::int32_t rate)
    {
       Map[st].update(bit,rate);
    }
  protected:
    std::uint8_t st;
    LinearCounterLimit Map[NUMSTATES];
};

template <std::int32_t N>
class SSENL
{
  //enum {szmap=1<<NB};
  public:
    std::int32_t tscale,xscale;
    std::uint16_t p_quant;
    SSENL(std::int32_t scale=myDomain.max)
    :tscale(scale),xscale((2*tscale)/(N-1))
    {
      if (xscale==0) xscale=1;
      for (std::int32_t i=0;i<=N;i++)
      {
         std::int32_t x=myDomain.Inv(i*xscale-tscale);
         Map[0][i].p1=x;
         Map[1][i].p1=x;
      }
      lb=0;
    };
    std::int32_t Predict(std::int32_t p1)
    {
       std::int32_t pq=(std::min)(2*tscale,(std::max)(0,myDomain.Fwd(p1)+tscale));

       p_quant=pq/xscale;
       std::int32_t p_mod=pq-(p_quant*xscale); //%xscale;

       std::int32_t pl=Map[lb][p_quant].p1;
       std::int32_t ph=Map[lb][p_quant+1].p1;

       std::int32_t px=(pl*(xscale-p_mod)+ph*p_mod)/xscale;
       return std::clamp(px,1,PSCALEm);
    };
    void Update(std::int32_t bit,std::int32_t rate,bool updlb=true)
    {
       Map[lb][p_quant].update(bit,rate);
       Map[lb][p_quant+1].update(bit,rate);
       if (updlb) lb=bit;
    };
  protected:
    LinearCounter16 Map[2][N+1];
    std::int32_t lb;
};

#endif
