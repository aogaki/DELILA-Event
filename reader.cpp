#include <TChain.h>
#include <TF1.h>
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

#include "TChSettings.hpp"
#include "THitData.hpp"

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

TH2D *histTime[34];
TH1D *histSize;
void InitHists()
{
  for (auto i = 0; i < 34; i++) {
    histTime[i] =
        new TH2D(Form("histTime_%d", i),
                 Form("Time difference ID%02d and other detectors", i), 20000,
                 -1000, 1000, 151, -0.5, 150.5);
    histTime[i]->SetXTitle("[ns]");
    histTime[i]->SetYTitle("Detector ID");
  }

  histSize =
      new TH1D("histSize", "Number of hits in one event", 100, 0.5, 100.5);
}

Double_t GetCalibratedEnergy(const ChSettings_t &chSetting, const UShort_t &adc)
{
  return chSetting.p0 + chSetting.p1 * adc + chSetting.p2 * adc * adc +
         chSetting.p3 * adc * adc * adc;
}

// TH1 is thread safe.  But if you get something trouble, you can use mutex.
std::mutex histMutex;
std::mutex counterMutex;
ChSettingsVec_t chSettingsVec;
void AnalysisThread(TString fileName, uint32_t threadNo)
{
  ROOT::EnableThreadSafety();

  auto file = new TFile(fileName);
  auto tree = (TTree *)file->Get("Event_Tree");
  std::vector<THitData> *event = nullptr;
  tree->SetBranchAddress("Event", &event);

  UChar_t triggerID = 0;
  tree->SetBranchAddress("TriggerID", &triggerID);
  UChar_t multiplicity = 0;
  tree->SetBranchAddress("Multiplicity", &multiplicity);
  UChar_t gammaMultiplicity = 0;
  tree->SetBranchAddress("GammaMultiplicity", &gammaMultiplicity);
  UChar_t ejMultiplicity = 0;
  tree->SetBranchAddress("EJMultiplicity", &ejMultiplicity);
  UChar_t gsMultiplicity = 0;
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

    // if (isFissionTrigger) {
    if (true) {
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
        auto distance = chSettingsVec[hit.Board][hit.Channel].distance;
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
  auto settingsFileName = "./chSettings.json";
  chSettingsVec = TChSettings::GetChSettings(settingsFileName);
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

  std::vector<TH1D *> histVec;
  const auto nDetectors = histTime[0]->GetYaxis()->GetNbins();
  for (auto i = 0; i < nDetectors; i++) {
    auto bin = i + 1;
    auto name = Form("histDet%03d", i);
    histVec.push_back(
        (TH1D *)histTime[0]->ProjectionX(name, bin, bin)->Clone());
  }

  std::vector<double> timeVec;
  int counter = 0;
  for (auto &hist : histVec) {
    if (hist->GetEntries() > 0 && counter >= 16 && counter <= 65) {
      auto name = Form("f%d", counter);
      auto f1 = new TF1(name, "gaus");
      auto maxBin = hist->GetMaximumBin();
      auto height = hist->GetBinContent(maxBin);
      auto mean = hist->GetBinCenter(maxBin);
      auto sigma = 2;
      f1->SetParameters(height, mean, sigma);
      f1->SetRange(mean - sigma, mean + sigma);
      hist->Fit(f1, "RQ");

      //mean = f1->GetParameter(1);
      //sigma = f1->GetParameter(2);
      //f1->SetRange(mean - sigma * 1.2, mean + sigma * 1.2);
      //hist->Fit(f1, "RQ");

      timeVec.push_back(f1->GetParameter(1));
    } else if (hist->GetEntries() > 0 && counter >= 80) {
      timeVec.push_back(-400.);
    } else {
      timeVec.push_back(0.);
    }

    counter++;
  }

  for (auto i = 0; i < timeVec.size(); i++) {
    std::cout << i << "\t" << timeVec[i] << std::endl;
  }

  std::ifstream fin(settingsFileName);
  nlohmann::json j;
  fin >> j;
  for (auto &&mod : j) {
    for (auto &&ch : mod) {
      auto timeOffset = ch["TimeOffset"].template get<double>();
      // auto timeOffset = ch["TimeOffset"];
      ch["TimeOffset"] = timeOffset - timeVec[ch["DetectorID"]];
    }
  }
  // std::cout << j.dump(4) << std::endl;
  std::ofstream fout("tmp.json");
  fout << j.dump(4) << std::endl;

  histTime[0]->Draw();
}
