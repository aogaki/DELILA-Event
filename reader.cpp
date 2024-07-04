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

TH2D *hist[34];
void InitHists()
{
  for (auto i = 0; i < 34; i++) {
    hist[i] = new TH2D(Form("hist_%d", i), Form("hist_%d", i), 20000, -1000,
                       1000, 112, -0.5, 111.5);
  }
}

// TH1 is thread safe.  But if you get something trouble, you can use mutex.
std::mutex histMutex;
std::mutex counterMutex;
ULong64_t totalEvents = 0;
ULong64_t processedEvents = 0;
void AnalysisThread(TString fileName, uint32_t threadNo)
{
  ROOT::EnableThreadSafety();

  auto file = new TFile(fileName);
  auto tree = (TTree *)file->Get("Event_Tree");
  std::vector<THitClass> *hits = nullptr;
  tree->SetBranchAddress("Event", &hits);

  for (auto i = 0; i < tree->GetEntries(); i++) {
    tree->GetEntry(i);
    bool processFlag = false;
    auto ch = -1;
    for (auto &hit : *hits) {
      if (hit.Timestamp == 0.) {
        processFlag = true;
        ch = hit.Board * 16 + hit.Channel;
        break;
      }
    }

    if (processFlag) {
      for (auto &hit : *hits) {
        if (hit.Timestamp != 0.) {
          auto id = hit.Board * 16 + hit.Channel;
          if (ch < 34) hist[ch]->Fill(hit.Timestamp, id);
        }
      }
    }

    std::lock_guard<std::mutex> lock(counterMutex);
    processedEvents++;
    if (processedEvents % 1000000 == 0) {
      std::cout << "\r" << processedEvents << " / " << totalEvents
                << std::flush;
    }
  }

  delete file;
}

void reader()
{
  gSystem->Load("./libEveBuilder.so");
  InitHists();

  auto fileList = GetFileList("./");
  for (auto &fileName : fileList) {
    auto file = new TFile(fileName.c_str());
    auto tree = (TTree *)file->Get("Event_Tree");
    totalEvents += tree->GetEntries();
    file->Close();
    delete file;
  }

  std::vector<std::thread> threads;
  for (auto i = 0; i < fileList.size(); i++) {
    threads.emplace_back(AnalysisThread, fileList[i], i);
  }

  for (auto &thread : threads) {
    thread.join();
  }

  std::cout << std::endl;

  hist[0]->Draw("COLZ");
}
