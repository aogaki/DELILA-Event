#ifndef TTimeOffset_hpp
#define TTimeOffset_hpp 1

// Checking the time offset between modules.
// Output files are:
// - timeOffset.root
// - timeOffset.json
// Those files show the time offset modN - mod0 for each module N.

#include <TH1.h>

#include <string>
#include <vector>

#include "TModSettings.hpp"

class TTimeOffset
{
 public:
  TTimeOffset(){};
  TTimeOffset(ModSettingsVec_t modSettings, std::vector<std::string> fileList);
  ~TTimeOffset(){};

  void CalculateTimeOffset();

  void SaveResults();

  void UpdateModSettings(ModSettingsVec_t &modSettings);
  ModSettingsVec_t GetModSettingsVec() { return fModSettingsVec; }

 private:
  ModSettingsVec_t fModSettingsVec;
  std::vector<std::vector<bool>> fIsPulserChVec;
  std::vector<double> fPulseDelayVec;

  std::vector<std::string> fFileList;
  std::vector<TH1D *> fHistTimeOffsetVec;
  void SaveHists();
  void SaveJSON();
  const std::string fOutputName = "timeOffset";

  uint32_t fNMods;
  void CheckNMods();
};

#endif
