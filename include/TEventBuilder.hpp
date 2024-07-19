#ifndef TEventBuilder_HPP
#define TEventBuilder_HPP 1

#include <TFile.h>
#include <TString.h>
#include <TTree.h>

#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "TChSettings.hpp"
#include "THitData.hpp"
#include "THitLoader.hpp"

class TEventBuilder
{
 public:
  TEventBuilder(){};
  TEventBuilder(Double_t timeWindow, ChSettingsVec_t chSettingsVec,
                std::vector<std::string> fileList, HitFileType hitType);
  ~TEventBuilder(){};

  void BuildEvent(uint32_t nFiles = 10, uint32_t nThreads = 16);

 private:
  Double_t GetCalibratedEnergy(const ChSettings_t &chSetting,
                               const UShort_t &adc);

  Double_t fTimeWindow = 1000;  // in ns
  void SearchAndWriteELIGANTEvents(uint32_t nThreads = 16,
                                   bool firstRun = false);

  std::vector<std::string> fFileList;
  ChSettingsVec_t fChSettingsVec;
  std::unique_ptr<std::vector<HitData_t>> fHitVec;
  HitFileType fHitType = HitFileType::DELILA;
};

#endif