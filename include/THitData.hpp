#ifndef THitData_hpp
#define THitData_hpp 1

#include <TROOT.h>

#include <tuple>

// Brd, Ch, FineTS, Energy, EnergyShort
typedef std::tuple<UChar_t, UChar_t, Double_t, UShort_t, UShort_t> HitData_t;

class THitData : public TObject
{
 public:
  UChar_t Channel;
  UChar_t Board;
  Double_t Timestamp;
  UShort_t Energy;
  UShort_t EnergyShort;

  THitData() {};
  THitData(UChar_t Board, UChar_t Channel, Double_t Timestamp, UShort_t Energy,
           UShort_t EnergyShort)
      : Channel(Channel),
        Timestamp(Timestamp),
        Board(Board),
        Energy(Energy),
        EnergyShort(EnergyShort) {};
  THitData(HitData_t hitData)
      : Board(std::get<0>(hitData)),
        Channel(std::get<1>(hitData)),
        Timestamp(std::get<2>(hitData)),
        Energy(std::get<3>(hitData)),
        EnergyShort(std::get<4>(hitData)) {};
  virtual ~THitData() {};

  ClassDef(THitData, 1);
};

#endif