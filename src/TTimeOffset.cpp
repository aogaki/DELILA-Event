#include "TTimeOffset.hpp"

#include <TFile.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

TTimeOffset::TTimeOffset(ModSettingsVec_t modSettings,
                         std::vector<std::string> fileList)
{
  fFileList = fileList;
  for (const auto &file : fFileList) {
    std::cout << file << std::endl;
  }

  fModSettingsVec = modSettings;
  std::sort(fModSettingsVec.begin(), fModSettingsVec.end(),
            [](const ModSettings_t &a, const ModSettings_t &b) {
              return a.mod < b.mod;
            });

  // Set parameters
  for (const auto &modSettings : fModSettingsVec) {
    std::vector<bool> isPulserChVec(modSettings.nChs, false);
    for (auto i = 0; i < modSettings.nChs; i++) {
      if (i == modSettings.pulserCh) {
        isPulserChVec[i] = true;
      }
    }
    fIsPulserChVec.push_back(isPulserChVec);
    fPulseDelayVec.push_back(modSettings.pulserDelay * 1000.);
  }

  CheckNMods();
}

void TTimeOffset::CheckNMods()
{
  auto fileName = fFileList[0];
  auto file = new TFile(fileName.c_str(), "READ");
  auto tree = (TTree *)file->Get("ELIADE_Tree");
  tree->SetBranchStatus("*", kFALSE);
  UChar_t mod;
  tree->SetBranchStatus("Mod", kTRUE);
  tree->SetBranchAddress("Mod", &mod);

  fNMods = 0;
  for (auto i = 0; i < tree->GetEntries(); i++) {
    tree->GetEntry(i);
    if (mod > fNMods) {
      fNMods = mod;
    }
  }

  fNMods++;
  std::cout << "Number of modules: " << fNMods << std::endl;

  if (fNMods != fModSettingsVec.size()) {
    std::cerr << "Number of modules in the settings file is different from the "
                 "number of modules in the data file."
              << std::endl;
    std::cerr << "Please check the settings file." << std::endl;
    exit(1);
  }

  delete file;
}

void TTimeOffset::CalculateTimeOffset()
{
  for (auto i = 0; i < fNMods; i++) {
    auto hist =
        new TH1D(Form("histTimeOffsetMod%d", i),
                 Form("Time offset for module %d", i), 10000, -5000, 5000);
    fHistTimeOffsetVec.push_back(hist);
  }

  for (const auto &fileName : fFileList) {
    auto file = TFile(fileName.c_str(), "READ");
    auto tree = (TTree *)file.Get("ELIADE_Tree");
    tree->SetBranchStatus("*", kFALSE);
    UChar_t mod, ch;
    Double_t ts;
    tree->SetBranchStatus("Mod", kTRUE);
    tree->SetBranchStatus("Ch", kTRUE);
    tree->SetBranchStatus("FineTS", kTRUE);
    tree->SetBranchAddress("Mod", &mod);
    tree->SetBranchAddress("Ch", &ch);
    tree->SetBranchAddress("FineTS", &ts);

    std::vector<Double_t> tsVec(fNMods, 0.);
    Double_t lastTS = -1.;
    for (auto i = 0; i < tree->GetEntries(); i++) {
      tree->GetEntry(i);
      constexpr auto timeGap = 10.e6;  // 10 us in ps order

      if (fIsPulserChVec[mod][ch]) {
        tsVec[mod] = ts - fPulseDelayVec[mod];

        if (!(lastTS < 0. || (ts - lastTS) < timeGap)) {
          for (auto j = 0; j < tsVec.size(); j++) {
            fHistTimeOffsetVec[j]->Fill((tsVec[j] - tsVec[0]) / 1000.);
          }
        }

        lastTS = ts;
      }
    }

    file.Close();
  }
}

void TTimeOffset::SaveResults()
{
  for (auto i = 0; i < fModSettingsVec.size(); i++) {
    fModSettingsVec[i].timeOffset = fHistTimeOffsetVec[i]->GetMean();
  }

  SaveHists();
  SaveJSON();
}

void TTimeOffset::SaveHists()
{
  auto fileName = fOutputName + ".root";
  auto outFile = new TFile(fileName.c_str(), "RECREATE");
  for (const auto &hist : fHistTimeOffsetVec) {
    hist->Write();
  }
  outFile->Close();
  delete outFile;
}

void TTimeOffset::SaveJSON()
{
  using json = nlohmann::json;
  json output;
  for (auto i = 0; i < fHistTimeOffsetVec.size(); i++) {
    json tmp;
    tmp["Module"] = i;
    tmp["TimeOffset"] = fHistTimeOffsetVec[i]->GetMean();
    output.push_back(tmp);
  }

  auto fileName = fOutputName + ".json";
  std::ofstream ofs(fileName);
  ofs << output.dump(4) << std::endl;
  ofs.close();
}

void TTimeOffset::UpdateModSettings(ModSettingsVec_t &modSettings)
{
  auto fileName = fOutputName + ".json";
  std::ifstream ifs(fileName);
  if (!ifs) {
    std::cerr << "File not found: " << fileName << std::endl;
    std::cerr << "Please calculate time offset and save it first." << std::endl;
    exit(1);
  }

  nlohmann::json j;
  ifs >> j;

  for (auto i = 0; i < modSettings.size(); i++) {
    if (modSettings[i].mod != j[i]["Module"]) {
      std::cerr << "Module number is different between the settings and the "
                   "time offset file."
                << std::endl;
      std::cerr << "Please check the settings file." << std::endl;
      exit(1);
    }
    modSettings[i].timeOffset = j[i]["TimeOffset"];
  }
}