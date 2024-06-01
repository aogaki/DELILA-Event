#include "TEventBuilder.hpp"

#include <TROOT.h>
#include <unistd.h>

#include <parallel/algorithm>

TEventBuilder::TEventBuilder(ChSettingsVec_t chSettingsVec,
                             ModSettingsVec_t modSettingsVec,
                             std::vector<std::string> fileList)
{
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
      // if (fFileList.size() < 465) {
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
        auto tree = dynamic_cast<TTree *>(file->Get("ELIADE_Tree"));

        tree->SetBranchStatus("*", kFALSE);
        UChar_t mod, ch;
        tree->SetBranchStatus("Mod", kTRUE);
        tree->SetBranchAddress("Mod", &mod);
        tree->SetBranchStatus("Ch", kTRUE);
        tree->SetBranchAddress("Ch", &ch);

        UShort_t adc;
        tree->SetBranchStatus("ChargeLong", kTRUE);
        tree->SetBranchAddress("ChargeLong", &adc);

        Double_t ts;
        tree->SetBranchStatus("FineTS", kTRUE);
        tree->SetBranchAddress("FineTS", &ts);

        auto hitsVec = std::make_unique<std::vector<DELILAHit>>();
        hitsVec->reserve(tree->GetEntries());
        for (auto i = 0; i < tree->GetEntries(); i++) {
          tree->GetEntry(i);
          hitsVec->emplace_back(mod, ch, adc,
                                ts - fModSettingsVec[mod].timeOffset * 1000.);
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
                       [](const DELILAHit &a, const DELILAHit &b) {
                         return a.TimeStamp < b.TimeStamp;
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
      auto fileName = Form("event_run%d_t%d.root", runNo, i);
      auto treeName = Form("Event_Tree");
      TFile *file = nullptr;
      TTree *tree = nullptr;
      std::vector<DELILAHit> *event = new std::vector<DELILAHit>();
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
        if (fChSettingsVec[hit.Module][hit.Channel].isEventTrigger) {
          const auto eventTS = hit.TimeStamp;
          event->emplace_back(DELILAHit(hit.Module, hit.Channel, hit.Energy,
                                        hit.TimeStamp - eventTS));
          // Search for hits in the past
          for (auto k = j; k >= 0; k--) {
            auto hitPast = fHitVec.at(k);
            if (hitPast.TimeStamp < eventTS - fTimeWindow / 2) {
              break;
            }
            event->emplace_back(DELILAHit(hitPast.Module, hitPast.Channel,
                                          hitPast.Energy,
                                          hitPast.TimeStamp - eventTS));
          }

          // Search for hits in the future
          for (auto k = j + 1; k < fHitVec.size(); k++) {
            auto hitFuture = fHitVec.at(k);
            if (hitFuture.TimeStamp > eventTS + fTimeWindow / 2) {
              break;
            }
            event->emplace_back(DELILAHit(hitFuture.Module, hitFuture.Channel,
                                          hitFuture.Energy,
                                          hitFuture.TimeStamp - eventTS));
          }

          std::sort(event->begin(), event->end(),
                    [](const DELILAHit &a, const DELILAHit &b) {
                      return a.TimeStamp < b.TimeStamp;
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