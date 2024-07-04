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

#include "DELILAHit.hpp"
#include "TChSettings.hpp"
#include "TELIGANTSettings.hpp"
#include "THitClass.hpp"
#include "TModSettings.hpp"

class TEventBuilder
{
 public:
  TEventBuilder(){};
  TEventBuilder(Double_t timeWindow, ELIGANTSettingsVec_t chSettingsVec,
                ModSettingsVec_t modSettingsVec,
                std::vector<std::string> fileList);
  ~TEventBuilder(){};

  void BuildEvent(uint32_t runNo, uint32_t nFiles = 10, uint32_t nThreads = 16);

 private:
  Double_t fTimeWindow = 1000;  // in ns
  void SearchAndWriteEvents(uint32_t runNo, uint32_t nThreads = 16,
                            bool firstRun = false);

  std::vector<std::string> fFileList;
  ModSettingsVec_t fModSettingsVec;
  ELIGANTSettingsVec_t fChSettingsVec;

  std::vector<THitClass> fHitVec;
  std::mutex fHitVecMutex;
  std::mutex fFileListMutex;
  void LoadHitsMT(uint32_t nFiles = 1, uint32_t nThreads = 16);
};

#endif