#include "TEventBuilder.hpp"

#include <TROOT.h>
#include <unistd.h>

#include <parallel/algorithm>

TEventBuilder::TEventBuilder(Double_t timeWindow, ChSettingsVec_t chSettingsVec,
                             ModSettingsVec_t modSettingsVec,
                             std::vector<std::string> fileList)
{
  fTimeWindow = timeWindow;
  fChSettingsVec = chSettingsVec;
  fModSettingsVec = modSettingsVec;
  fFileList = fileList;
}

void TEventBuilder::BuildEvent(uint32_t runNo, uint32_t nFiles,
                               uint32_t nThreads)
{
  bool firstRun = true;
  while (true) {
    if (fFileList.size() == 0) {
      break;
    }

    LoadHitsMT(nFiles);

    SearchAndWriteEvents(runNo, nThreads, firstRun);
    firstRun = false;
  }
}

void TEventBuilder::LoadHitsMT(uint32_t nFiles)
{
  ROOT::EnableThreadSafety();

  fHitVec.clear();
  uint32_t nThreads = nFiles;
  std::vector<std::thread> threads;
  for (auto i = 0; i < nThreads; i++) {
    threads.emplace_back([this, i, nFiles, nThreads]() {
      for (auto j = i; j < nFiles; j += nThreads) {
        TString fileName;
        {
          std::lock_guard<std::mutex> lock(fHitVecMutex);
          if (fFileList.size() == 0) {
            std::cerr << "No more file to load" << std::endl;
            break;
          }
          fileName = fFileList[0];
          fFileList.erase(fFileList.begin());
        }
        std::cout << "Loading file: " << fileName << std::endl;
        auto file = TFile::Open(fileName, "READ");
        if (!file) {
          std::cerr << "File not found: " << fileName << std::endl;
          break;
        }
        auto tree = dynamic_cast<TTree *>(file->Get("tout"));

        tree->SetBranchStatus("*", kFALSE);
        UShort_t brd;
        tree->SetBranchStatus("Board", kTRUE);
        tree->SetBranchAddress("Board", &brd);
        UShort_t ch;
        tree->SetBranchStatus("Channel", kTRUE);
        tree->SetBranchAddress("Channel", &ch);

        UShort_t ene;
        tree->SetBranchStatus("Energy", kTRUE);
        tree->SetBranchAddress("Energy", &ene);

        UShort_t eneShort;
        tree->SetBranchStatus("EnergyShort", kTRUE);
        tree->SetBranchAddress("EnergyShort", &eneShort);

        ULong64_t ts;
        tree->SetBranchStatus("Timestamp", kTRUE);
        tree->SetBranchAddress("Timestamp", &ts);

        UInt_t flag;
        tree->SetBranchStatus("Flags", kTRUE);
        tree->SetBranchAddress("Flags", &flag);

        auto hitsVec = std::make_unique<std::vector<THitClass>>();
        hitsVec->reserve(tree->GetEntries());
        for (auto i = 0; i < tree->GetEntries(); i++) {
          tree->GetEntry(i);
          if (flag == 0) continue;
          hitsVec->emplace_back(ch, Double_t(ts), brd, ene, eneShort);
        }

        file->Close();

        std::lock_guard<std::mutex> lock(fHitVecMutex);
        fHitVec.insert(fHitVec.end(), std::make_move_iterator(hitsVec->begin()),
                       std::make_move_iterator(hitsVec->end()));
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  __gnu_parallel::sort(fHitVec.begin(), fHitVec.end(),
                       [](const THitClass &a, const THitClass &b) {
                         return a.Timestamp < b.Timestamp;
                       });
  std::cout << fHitVec.size() << " hits  loaded" << std::endl;
}

void TEventBuilder::SearchAndWriteEvents(uint32_t runNo, uint32_t nThreads,
                                         bool firstRun)
{
  ROOT::EnableThreadSafety();

  std::vector<std::thread> threads;
  for (auto i = 0; i < nThreads; i++) {
    threads.emplace_back([this, i, nThreads, runNo, firstRun]() {
      auto fileName = Form("event_t%d.root", i);
      auto treeName = Form("Event_Tree");
      TFile *file = nullptr;
      TTree *tree = nullptr;
      std::vector<THitClass> *event = new std::vector<THitClass>();
      if (firstRun) {
        file = TFile::Open(fileName, "RECREATE");
        tree = new TTree(treeName, "Event Tree");
        tree->Branch("Event", &event);
      } else {
        file = TFile::Open(fileName, "UPDATE");
        tree = dynamic_cast<TTree *>(file->Get(treeName));
        tree->SetBranchAddress("Event", &event);
      }
      tree->SetDirectory(file);

      for (auto j = i; j < fHitVec.size(); j += nThreads) {
        auto hit = fHitVec.at(j);
        if (hit.Board == 0 && hit.Channel == 0) {
          const Double_t eventTS = hit.Timestamp;
          event->emplace_back(hit.Channel, hit.Timestamp, hit.Board, hit.Energy,
                              hit.EnergyShort);
          // Search for hits in the past
          for (auto k = j; k >= 0; k--) {
            auto hitPast = fHitVec.at(k);
            if (hitPast.Timestamp < eventTS - fTimeWindow / 2) {
              break;
            }
            event->emplace_back(hit.Channel, hit.Timestamp, hit.Board,
                                hit.Energy, hit.EnergyShort);
          }

          // Search for hits in the future
          for (auto k = j + 1; k < fHitVec.size(); k++) {
            auto hitFuture = fHitVec.at(k);
            if (hitFuture.Timestamp > eventTS + fTimeWindow / 2) {
              break;
            }
            event->emplace_back(hit.Channel, hit.Timestamp, hit.Board,
                                hit.Energy, hit.EnergyShort);
          }

          std::sort(event->begin(), event->end(),
                    [](const THitClass &a, const THitClass &b) {
                      return a.Timestamp < b.Timestamp;
                    });

          tree->Fill();
          event->clear();
        }
      }

      tree->Write();
      // file->Close();
      delete file;
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  fHitVec.clear();
}