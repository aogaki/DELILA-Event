#include <TChain.h>
#include <TFile.h>
#include <TROOT.h>
#include <TString.h>
#include <TSystem.h>
#include <TTree.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <parallel/algorithm>
#include <vector>

#include "DELILAHit.hpp"
#include "TChSettings.hpp"
#include "TEventBuilder.hpp"
#include "TModSettings.hpp"
#include "TTimeOffset.hpp"

std::vector<std::string> GetFileList(const std::string dirName, const int runNo)
{
  std::vector<std::string> tmpList;

  auto searchKey = Form("run%d_", runNo);
  for (const auto &entry : std::filesystem::directory_iterator(dirName)) {
    if (entry.path().string().find(searchKey) != std::string::npos) {
      tmpList.push_back(entry.path().string());
    }
  }

  std::vector<std::string> fileList;
  for (auto subRunNo = 0; subRunNo < tmpList.size(); subRunNo++) {
    // Too stupid, think better way
    searchKey = Form("run%d_%d_", runNo, subRunNo);
    for (auto fileName : tmpList) {
      if (fileName.find(searchKey) != std::string::npos) {
        fileList.push_back(fileName);
      }
    }
  }

  return fileList;
}

ModSettingsVec_t GetModSettings(const std::string fileName)
{
  ModSettingsVec_t modSettingsVec;

  std::ifstream ifs(fileName);
  if (!ifs) {
    std::cerr << "File not found: " << fileName << std::endl;
    return modSettingsVec;
  }

  nlohmann::json j;
  ifs >> j;

  for (const auto &mod : j) {
    ModSettings_t modSettings;
    modSettings.mod = mod["Module"];
    modSettings.nChs = mod["NChannels"];
    modSettings.FW = mod["FW"];
    modSettings.pulserDelay = mod["PulserDelay"];
    modSettings.timeOffset = mod["TimeOffset"];
    modSettings.pulserCh = mod["PulserCh"];
    // modSettings.Print();
    modSettingsVec.push_back(modSettings);
  }

  return modSettingsVec;
}

ChSettingsVec_t GetChSettings(const std::string fileName)
{
  ChSettingsVec_t chSettingsVec;

  std::ifstream ifs(fileName);
  if (!ifs) {
    std::cerr << "File not found: " << fileName << std::endl;
    return chSettingsVec;
  }

  nlohmann::json j;
  ifs >> j;

  for (const auto &mod : j) {
    std::vector<ChSettings_t> chSettings;
    for (const auto &ch : mod) {
      ChSettings_t chSetting;
      chSetting.mod = ch["Module"];
      chSetting.ch = ch["Channel"];
      chSetting.ACMod = ch["ACModule"];
      chSetting.ACCh = ch["ACChannel"];
      chSetting.isPMT = ch["IsPMT"];
      chSetting.isEventTrigger = ch["IsEventTrigger"];
      chSetting.hasAC = ch["HasAC"];
      // chSetting.Print();
      chSettings.push_back(chSetting);
    }
    chSettingsVec.push_back(chSettings);
  }

  return chSettingsVec;
}

enum class RunMode { TimeOffset, EventBuild };

int main(int argc, char *argv[])
{
  std::string dirName = "/home/aogaki/DAQ/DELILA-Event/data";
  int runNo = 43;
  RunMode runMode = RunMode::EventBuild;
  uint32_t nFiles = 10;
  uint32_t nThreads = 16;
  Double_t timeWindow = 1000;  // in ns
  // -d is directory name
  // -r is run number
  // -t is time offset mode
  // -f is number of files to be processed in one loop
  // -n is number of threads
  // -w is time window in ns
  // -h is help
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "-d") {
      dirName = argv[i + 1];
    }
    if (std::string(argv[i]) == "-r") {
      runNo = std::stoi(argv[i + 1]);
    }
    if (std::string(argv[i]) == "-t") {
      runMode = RunMode::TimeOffset;
    }
    if (std::string(argv[i]) == "-f") {
      nFiles = std::stoi(argv[i + 1]);
    }
    if (std::string(argv[i]) == "-n") {
      nThreads = std::stoi(argv[i + 1]);
    }
    if (std::string(argv[i]) == "-w") {
      timeWindow = std::stod(argv[i + 1]);
    }
    if (std::string(argv[i]) == "-h") {
      std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  -d <directory name> : Set directory name" << std::endl;
      std::cout << "  -r <run number> : Set run number" << std::endl;
      std::cout << "  -t : Time offset mode" << std::endl;
      std::cout << "  -f <number of files> : Set number of files to be "
                   "processed in one loop"
                << std::endl;
      std::cout << "  -n <number of threads> : Set number of threads"
                << std::endl;
      std::cout << "  -w <time window in ns> : Set time window in ns"
                << std::endl;
      std::cout << "  -h : Show this help" << std::endl;
      return 0;
    }
  }

  auto fileList = GetFileList(dirName, runNo);
  if (fileList.size() == 0) {
    std::cerr << "No files found." << std::endl;
    return 1;
  }

  auto modSettingsVec = GetModSettings("modSettings.json");
  if (modSettingsVec.size() == 0) {
    std::cerr << "No module settings file \"modSettings.json\" found."
              << std::endl;
    return 1;
  }

  auto chSettingsVec = GetChSettings("chSettings.json");
  if (chSettingsVec.size() == 0) {
    std::cerr << "No channel settings file \"chSettings.json\" found."
              << std::endl;
    return 1;
  }

  if (runMode == RunMode::TimeOffset) {
    TTimeOffset timeOffset(modSettingsVec, fileList);
    timeOffset.CalculateTimeOffset();
    timeOffset.SaveResults();
    modSettingsVec = timeOffset.GetModSettingsVec();
  } else {
    std::cout << "Loading time offset from \"timeOffset.json\"." << std::endl;
    TTimeOffset timeOffset;
    timeOffset.UpdateModSettings(modSettingsVec);

    auto builder =
        TEventBuilder(timeWindow, chSettingsVec, modSettingsVec, fileList);
    builder.BuildEvent(runNo, nFiles, nThreads);
  }

  return 0;
}