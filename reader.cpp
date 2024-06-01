#include <TChain.h>
#include <TFile.h>
#include <TH1.h>
#include <TH2.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TTree.h>

#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "DELILAHit.hpp"

std::vector<std::string> GetFileList(const std::string dirName, const int runNo)
{
  std::vector<std::string> fileList;

  auto searchKey = Form("event_run%d_", runNo);
  // auto searchKey = Form("event_run%d_s", runNo);
  for (const auto &entry : std::filesystem::directory_iterator(dirName)) {
    if (entry.path().string().find(searchKey) != std::string::npos) {
      fileList.push_back(entry.path().string());
    }
  }

  return fileList;
}

TH2D *hist[17];
void InitHists()
{
  for (auto i = 0; i < 17; i++) {
    hist[i] = new TH2D(Form("hist_%d", i), Form("hist_%d", i), 10000, -500, 500,
                       10, -0.5, 9.5);
  }
}

// TH1 is thread safe.  But if you get something trouble, you can use mutex.
std::mutex histMutex;
void AnalysisThread(TString fileName, uint32_t threadNo)
{
  ROOT::EnableThreadSafety();

  auto file = new TFile(fileName);
  auto tree = (TTree *)file->Get("Event_Tree");
  std::vector<DELILAHit> *hits = nullptr;
  tree->SetBranchAddress("Event", &hits);

  for (auto i = 0; i < tree->GetEntries(); i++) {
    if (i != 0 && i % 1000000 == 0) {
      std::lock_guard<std::mutex> lock(histMutex);
      std::cout << Form("Thread %02d: ", threadNo) << i << " / "
                << tree->GetEntries() << std::endl;
    }

    tree->GetEntry(i);
    auto ch = -1;
    for (auto &hit : *hits) {
      if (hit.TimeStamp == 0. && hit.Module == 0) {
        ch = hit.Channel;
        break;
      }
    }

    if (ch != -1) {
      for (auto &hit : *hits) {
        if (hit.TimeStamp != 0.) {
          hist[ch]->Fill(hit.TimeStamp / 1000., hit.Module);
          hist[16]->Fill(hit.TimeStamp / 1000., hit.Module);
        }
      }
    }
  }

  delete file;
}

void reader()
{
  gSystem->Load("./libEveBuilder.so");
  InitHists();

  auto fileList = GetFileList("./", 95);

  std::vector<std::thread> threads;
  for (auto i = 0; i < fileList.size(); i++) {
    threads.emplace_back(AnalysisThread, fileList[i], i);
  }

  for (auto &thread : threads) {
    thread.join();
  }
}
