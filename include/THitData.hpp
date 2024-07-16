#ifndef THitData_hpp
#define THitData_hpp 1

#include <TROOT.h>

class THitData : public TObject
{
 public:
  UShort_t Channel;
  // ULong64_t Timestamp;
  Double_t Timestamp;
  UShort_t Board;
  UShort_t Energy;
  UShort_t EnergyShort;

  THitData(){};
  THitData(UShort_t Channel, Double_t Timestamp, UShort_t Board,
           UShort_t Energy, UShort_t EnergyShort)
  {
    this->Channel = Channel;
    this->Timestamp = Timestamp;
    this->Board = Board;
    this->Energy = Energy;
    this->EnergyShort = EnergyShort;
  }
  virtual ~THitData(){};

  ClassDef(THitData, 1);
};

#endif