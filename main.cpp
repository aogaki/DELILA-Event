#include <TChain.h>
#include <TFile.h>
#include <TROOT.h>
#include <TString.h>
#include <TSystem.h>
#include <TTree.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <parallel/algorithm>
#include <string>
#include <vector>

#include "TChSettings.hpp"
#include "TEventBuilder.hpp"
#include "THitData.hpp"
#include "THitLoader.hpp"

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

  auto detectorID = 0;

  for (const auto &mod : j) {
    std::vector<ChSettings_t> chSettings;
    for (const auto &ch : mod) {
      ChSettings_t chSetting;
      chSetting.isEventTrigger = ch["IsEventTrigger"];
      chSetting.mod = ch["Module"];
      chSetting.ch = ch["Channel"];
      chSetting.timeOffset = ch["TimeOffset"];
      chSetting.hasAC = ch["HasAC"];
      chSetting.ACMod = ch["ACModule"];
      chSetting.ACCh = ch["ACChannel"];
      chSetting.phi = ch["Phi"];
      chSetting.theta = ch["Theta"];
      chSetting.distance = ch["Distance"];
      chSetting.x = ch["x"];
      chSetting.y = ch["y"];
      chSetting.z = ch["z"];
      chSetting.p0 = ch["p0"];
      chSetting.p1 = ch["p1"];
      chSetting.p2 = ch["p2"];
      chSetting.p3 = ch["p3"];
      if (chSetting.isEventTrigger) {
        chSetting.detectorID = detectorID;
        detectorID++;
      } else {
        chSetting.detectorID = -1;
      }
      chSettings.push_back(chSetting);
    }
    chSettingsVec.push_back(chSettings);
  }

  return chSettingsVec;
}

int main(int argc, char *argv[])
{
  uint32_t nFiles = 0;
  uint32_t nFilesLoop = 10;
  uint32_t nThreads = 16;
  Double_t timeWindow = 2000;  // in ns
  HitFileType hitFileType = HitFileType::DELILA;
  auto fileListName = std::string(argv[argc - 1]);
  // -f is number of files to be processed
  // -l is number of files to be processed in one loop
  // -t is number of threads
  // -w is time window in ns
  // -d is daq type
  // -h is help
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "-l") {
      nFilesLoop = std::stoi(argv[i + 1]);
    }
    if (std::string(argv[i]) == "-f") {
      nFiles = std::stoi(argv[i + 1]);
    }
    if (std::string(argv[i]) == "-t") {
      nThreads = std::stoi(argv[i + 1]);
    }
    if (std::string(argv[i]) == "-w") {
      timeWindow = std::stod(argv[i + 1]);
    }
    if (std::string(argv[i]) == "-d") {
      if (std::string(argv[i + 1]) == "ELIGANT") {
        hitFileType = HitFileType::ELIGANT;
      } else if (std::string(argv[i + 1]) == "DELILA") {
        hitFileType = HitFileType::DELILA;
      } else {
        std::cerr << "Unknown DAQ type: " << argv[i + 1] << std::endl;
        return 1;
      }
    }
    if (std::string(argv[i]) == "-h") {
      std::cout << "Usage: " << argv[0] << " [options] fileList" << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  -f <number of files> : Set number of files to be "
                   "processed"
                << std::endl;
      std::cout << "  -l <number of files> : Set number of files to be "
                   "processed in one loop"
                << std::endl;
      std::cout << "  -t <number of threads> : Set number of threads"
                << std::endl;
      std::cout << "  -w <time window in ns> : Set time window in ns"
                << std::endl;
      std::cout << "  -h : Show this help" << std::endl;
      std::cout << "To generate a file list, please use \"ls -v1 "
                   "somewhere/*\".  It makes "
                   "the file list in the correct order and format."
                << std::endl;
      return 0;
    }
  }

  std::vector<std::string> fileList;
  std::ifstream ifs(fileListName);
  if (!ifs) {
    std::cerr << "File not found: " << fileListName << std::endl;
    return 1;
  }
  std::string line;
  while (std::getline(ifs, line)) {
    fileList.push_back(line);
  }
  if (nFiles > 0 && nFiles < fileList.size()) {
    fileList.resize(nFiles);
  }

  if (nFilesLoop == 0) {
    nFilesLoop = fileList.size();  // probably use all memory and crash
  }

  auto chSettingsVec = GetChSettings("chSettings.json");
  if (chSettingsVec.size() == 0) {
    std::cerr << "No channel settings file \"chSettings.json\" found."
              << std::endl;
    return 1;
  }

  auto builder =
      TEventBuilder(timeWindow, chSettingsVec, fileList, hitFileType);
  builder.BuildEvent(nFilesLoop, nThreads);

  return 0;
}