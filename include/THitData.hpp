#ifndef THitData_hpp
#define THitData_hpp 1

#include <TROOT.h>

#include <tuple>

// Brd, Ch, FineTS, Energy, EnergyShort
typedef std::tuple<uint8_t, uint8_t, double_t, uint16_t, uint16_t> HitData_t;

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
  THitData(UShort_t Board, UShort_t Channel, Double_t Timestamp,
           UShort_t Energy, UShort_t EnergyShort)
      : Channel(Channel),
        Timestamp(Timestamp),
        Board(Board),
        Energy(Energy),
        EnergyShort(EnergyShort){};
  THitData(HitData_t hitData)
      : Channel(std::get<1>(hitData)),
        Timestamp(std::get<2>(hitData)),
        Board(std::get<0>(hitData)),
        Energy(std::get<3>(hitData)),
        EnergyShort(std::get<4>(hitData)){};
  virtual ~THitData(){};

  // ClassDef(THitData, 1);
};

#endif