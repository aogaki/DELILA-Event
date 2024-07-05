#include <TChain.h>
#include <TFile.h>
#include <TH1.h>
#include <TH2.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TTree.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "TELIGANTSettings.hpp"
#include "THitClass.hpp"

std::vector<std::string> GetFileList(const std::string dirName)
{
  std::vector<std::string> fileList;

  auto searchKey = Form("event_t");
  // auto searchKey = Form("event_run%d_s", runNo);
  for (const auto &entry : std::filesystem::directory_iterator(dirName)) {
    if (entry.path().string().find(searchKey) != std::string::npos) {
      fileList.push_back(entry.path().string());
    }
  }

  return fileList;
}

ELIGANTSettingsVec_t chSettingsVec;
ELIGANTSettingsVec_t GetELIGANTSettings(const std::string fileName)
{
  ELIGANTSettingsVec_t eligantSettingsVec;

  std::ifstream ifs(fileName);
  if (!ifs) {
    std::cerr << "File not found: " << fileName << std::endl;
    return eligantSettingsVec;
  }

  nlohmann::json j;
  ifs >> j;

  for (const auto &mod : j) {
    std::vector<ELIGANTSettings_t> eligantSettings;
    for (const auto &ch : mod) {
      ELIGANTSettings_t eligantSetting;
      eligantSetting.mod = ch["Module"];
      eligantSetting.ch = ch["Channel"];
      eligantSetting.timeOffset = ch["TimeOffset"];
      eligantSetting.isEventTrigger = ch["IsEventTrigger"];
      eligantSetting.phi = ch["Phi"];
      eligantSetting.theta = ch["Theta"];
      eligantSetting.x = ch["X"];
      eligantSetting.y = ch["Y"];
      eligantSetting.z = ch["Z"];
      eligantSetting.r = ch["Distance"];
      eligantSetting.p0 = ch["p0"];
      eligantSetting.p1 = ch["p1"];
      eligantSetting.p2 = ch["p2"];
      eligantSetting.p3 = ch["p3"];
      eligantSettings.push_back(eligantSetting);
    }
    eligantSettingsVec.push_back(eligantSettings);
  }

  return eligantSettingsVec;
}

TH2D *histTime[34];
TH1D *histSize;
void InitHists()
{
  for (auto i = 0; i < 34; i++) {
    histTime[i] =
        new TH2D(Form("histTime_%d", i),
                 Form("Time difference ID%02d and other detectors", i), 20000,
                 -1000, 1000, 112, -0.5, 111.5);
    histTime[i]->SetXTitle("[ns]");
    histTime[i]->SetYTitle("Detector ID");
  }

  histSize =
      new TH1D("histSize", "Number of hits in one event", 100, 0.5, 100.5);
}

Double_t GetCalibratedEnergy(const ELIGANTSettings_t &chSetting,
                             const UShort_t &adc)
{
  return chSetting.p0 + chSetting.p1 * adc + chSetting.p2 * adc * adc +
         chSetting.p3 * adc * adc * adc;
}

// TH1 is thread safe.  But if you get something trouble, you can use mutex.
std::mutex histMutex;
std::mutex counterMutex;
void AnalysisThread(TString fileName, uint32_t threadNo)
{
  ROOT::EnableThreadSafety();

  auto file = new TFile(fileName);
  auto tree = (TTree *)file->Get("Event_Tree");
  std::vector<THitClass> *event = nullptr;
  tree->SetBranchAddress("Event", &event);

  UShort_t triggerID = 0;
  tree->SetBranchAddress("TriggerID", &triggerID);
  UShort_t multiplicity = 0;
  tree->SetBranchAddress("Multiplicity", &multiplicity);
  UShort_t gammaMultiplicity = 0;
  tree->SetBranchAddress("GammaMultiplicity", &gammaMultiplicity);
  UShort_t ejMultiplicity = 0;
  tree->SetBranchAddress("EJMultiplicity", &ejMultiplicity);
  UShort_t gsMultiplicity = 0;
  tree->SetBranchAddress("GSMultiplicity", &gsMultiplicity);
  Bool_t isFissionTrigger = false;
  tree->SetBranchAddress("IsFissionTrigger", &isFissionTrigger);

  auto nEntries = tree->GetEntries();
  // nEntries /= 10;  // for test
  const auto startTime = std::chrono::system_clock::now();
  for (auto i = 0; i < nEntries; i++) {
    if (i != 0 && threadNo == 0 && i % 1000000 == 0) {
      auto currentTime = std::chrono::system_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         currentTime - startTime)
                         .count();
      auto remainingTime = (nEntries - i) * elapsed / i / 1.e9;
      std::cout << "\b\r" << Form("Thread %02d: ", threadNo) << i << " / "
                << nEntries << ", " << int(remainingTime) << "s  \b\b"
                << std::flush;
    }

    tree->GetEntry(i);

    if (isFissionTrigger) {
      for (auto &hit : *event) {
        auto id = hit.Board * 16 + hit.Channel;
        if (hit.Timestamp != 0.) {
          if (triggerID < 34) histTime[triggerID]->Fill(hit.Timestamp, id);
        }

        auto eneLong = GetCalibratedEnergy(
            chSettingsVec[hit.Board][hit.Channel], hit.Energy);
        auto eneShort = GetCalibratedEnergy(
            chSettingsVec[hit.Board][hit.Channel], hit.EnergyShort);
        auto PS = (eneLong - eneShort) / eneLong;
        auto x = chSettingsVec[hit.Board][hit.Channel].x;
        auto y = chSettingsVec[hit.Board][hit.Channel].y;
        auto z = chSettingsVec[hit.Board][hit.Channel].z;
        auto r = chSettingsVec[hit.Board][hit.Channel].r;
        auto theta = chSettingsVec[hit.Board][hit.Channel].theta;
        auto phi = chSettingsVec[hit.Board][hit.Channel].phi;
      }
    }
  }

  delete file;

  auto endTime = std::chrono::system_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime)
          .count();
  std::lock_guard<std::mutex> lock(counterMutex);
  std::cout << "Finished: " << fileName << ", " << elapsed / 1.e9 << "s"
            << std::endl;
}

void reader()
{
  gSystem->Load("./libEveBuilder.so");
  chSettingsVec = GetELIGANTSettings("./eligantSettings.json");
  InitHists();

  auto fileList = GetFileList("./");

  std::vector<std::thread> threads;
  for (auto i = 0; i < fileList.size(); i++) {
    threads.emplace_back(AnalysisThread, fileList[i], i);
  }

  for (auto &thread : threads) {
    thread.join();
  }

  std::cout << std::endl;

  histTime[0]->Draw("COLZ");
}
