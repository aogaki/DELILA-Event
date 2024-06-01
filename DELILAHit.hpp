#ifndef DELILAHit_HPP
#define DELILAHit_HPP

#include <TObject.h>

class DELILAHit : public TObject
{
 public:
  UChar_t Module;
  UChar_t Channel;
  UShort_t Energy;
  Double_t TimeStamp;

  DELILAHit(){};
  DELILAHit(UChar_t module, UChar_t channel, UShort_t energy, Double_t timestamp)
  {
    Module = module;
    Channel = channel;
    Energy = energy;
    TimeStamp = timestamp;
  };
  virtual ~DELILAHit(){};

  ClassDef(DELILAHit, 1);
};

typedef DELILAHit DELILAHit_t;
typedef std::vector<DELILAHit_t> DELILAEvent_t;

#endif