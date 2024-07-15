#include "TEventBuilder.hpp"

#include <TROOT.h>
#include <unistd.h>

#include <parallel/algorithm>

TEventBuilder::TEventBuilder(Double_t timeWindow,
                             ELIGANTSettingsVec_t chSettingsVec,
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

    LoadHitsMT(nFiles, nThreads);

    SearchAndWriteEvents(runNo, nThreads, firstRun);
    firstRun = false;
  }
}

Double_t TEventBuilder::GetCalibratedEnergy(const ELIGANTSettings_t &chSetting,
                                            const UShort_t &adc)
{
  return chSetting.p0 + chSetting.p1 * adc + chSetting.p2 * adc * adc +
         chSetting.p3 * adc * adc * adc;
}

void TEventBuilder::LoadHitsMT(uint32_t nFiles, uint32_t nThreads)
{
  ROOT::EnableThreadSafety();

  fHitVec.clear();
  // uint32_t nThreads = nFiles;
  std::vector<std::thread> threads;
  for (auto i = 0; i < nThreads; i++) {
    threads.emplace_back([this, i, nFiles, nThreads]() {
      for (auto j = i; j < nFiles; j += nThreads) {
        TString fileName;
        {
          std::lock_guard<std::mutex> lock(fFileListMutex);
          if (fFileList.size() == 0) {
            std::cerr << "No more file to load" << std::endl;
            break;
          }
          fileName = fFileList[0];
          fFileList.erase(fFileList.begin());
          std::cout << "Loading file: " << fileName << std::endl;
        }
        auto file = TFile::Open(fileName, "READ");
        if (!file) {
          std::cerr << "File not found: " << fileName << std::endl;
          break;
        }
        auto tree = dynamic_cast<TTree *>(file->Get("tout"));

        if (j == 0) {
          fHitVec.reserve(tree->GetEntries() * nFiles * 1.1);
        }

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
          Double_t fineTS =
              Double_t(ts) / 1000. + fChSettingsVec.at(brd).at(ch).timeOffset;
          hitsVec->emplace_back(ch, fineTS, brd, ene, eneShort);
        }

        file->Close();

        std::lock_guard<std::mutex> lock(fHitVecMutex);
        fHitVec.insert(fHitVec.end(), std::make_move_iterator(hitsVec->begin()),
                       std::make_move_iterator(hitsVec->end()));
        std::cout << fileName << " loaded" << std::endl;
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
      UShort_t triggerID;
      Double_t triggerTS;
      UShort_t multiplicity;
      UShort_t gammaMultiplicity;
      UShort_t ejMultiplicity;
      UShort_t gsMultiplicity;
      Bool_t isFissionTrigger;
      if (firstRun) {
        file = TFile::Open(fileName, "RECREATE");
        tree = new TTree(treeName, "Event Tree");
        tree->Branch("Event", &event);
        tree->Branch("TriggerID", &triggerID);
        tree->Branch("TriggerTS", &triggerTS);
        tree->Branch("Multiplicity", &multiplicity);
        tree->Branch("GammaMultiplicity", &gammaMultiplicity);
        tree->Branch("EJMultiplicity", &ejMultiplicity);
        tree->Branch("GSMultiplicity", &gsMultiplicity);
        tree->Branch("IsFissionTrigger", &isFissionTrigger);
      } else {
        file = TFile::Open(fileName, "UPDATE");
        tree = dynamic_cast<TTree *>(file->Get(treeName));
        tree->SetBranchAddress("Event", &event);
        tree->SetBranchAddress("TriggerID", &triggerID);
        tree->SetBranchAddress("TriggerTS", &triggerTS);
        tree->SetBranchAddress("Multiplicity", &multiplicity);
        tree->SetBranchAddress("GammaMultiplicity", &gammaMultiplicity);
        tree->SetBranchAddress("EJMultiplicity", &ejMultiplicity);
        tree->SetBranchAddress("GSMultiplicity", &gsMultiplicity);
        tree->SetBranchAddress("IsFissionTrigger", &isFissionTrigger);
      }
      tree->SetDirectory(file);

      for (auto j = i; j < fHitVec.size(); j += nThreads) {
        auto hit = fHitVec.at(j);
        if (fChSettingsVec.at(hit.Board).at(hit.Channel).isEventTrigger) {
          bool fillingFlag = true;

          triggerID = hit.Board * 16 + hit.Channel;
          triggerTS = hit.Timestamp;
          multiplicity = 0;
          gammaMultiplicity = 0;
          ejMultiplicity = 0;
          gsMultiplicity = 0;
          isFissionTrigger = false;

          double eneSum = GetCalibratedEnergy(
              fChSettingsVec.at(hit.Board).at(hit.Channel), hit.Energy);

          const Double_t eventTS = triggerTS;
          event->emplace_back(hit.Channel, 0, hit.Board, hit.Energy,
                              hit.EnergyShort);
          multiplicity++;
          if (triggerID < 34) {
            gammaMultiplicity++;
          } else if (47 < triggerID && triggerID < 85) {
            ejMultiplicity++;
          } else if (86 < triggerID && triggerID < 112) {
            gsMultiplicity++;
          }
          // Search for hits in the past
          if (fillingFlag && j > 0) {
            for (auto k = j - 1; k >= 0; k--) {
              auto hitPast = fHitVec.at(k);
              if (hitPast.Timestamp < eventTS - fTimeWindow / 2) {
                break;
              }
              event->emplace_back(hitPast.Channel, hitPast.Timestamp - eventTS,
                                  hitPast.Board, hitPast.Energy,
                                  hitPast.EnergyShort);

              eneSum += GetCalibratedEnergy(
                  fChSettingsVec.at(hitPast.Board).at(hitPast.Channel),
                  hitPast.Energy);

              int32_t id = hitPast.Board * 16 + hitPast.Channel;
              if (id < triggerID) {
                fillingFlag = false;
                break;
              }
              multiplicity++;
              if (id < 34) {
                gammaMultiplicity++;
              } else if (47 < id && id < 85) {
                ejMultiplicity++;
              } else if (86 < id && id < 112) {
                gsMultiplicity++;
              }
            }
          }

          // Search for hits in the future
          if (fillingFlag && j + 1 < fHitVec.size()) {
            for (auto k = j + 1; k < fHitVec.size(); k++) {
              auto hitFuture = fHitVec.at(k);
              if (hitFuture.Timestamp > eventTS + fTimeWindow / 2) {
                break;
              }
              event->emplace_back(
                  hitFuture.Channel, hitFuture.Timestamp - eventTS,
                  hitFuture.Board, hitFuture.Energy, hitFuture.EnergyShort);

              eneSum += GetCalibratedEnergy(
                  fChSettingsVec.at(hitFuture.Board).at(hitFuture.Channel),
                  hitFuture.Energy);

              int32_t id = hitFuture.Board * 16 + hitFuture.Channel;
              if (id < triggerID) {
                fillingFlag = false;
                break;
              }
              multiplicity++;
              if (id < 34) {
                gammaMultiplicity++;
              } else if (47 < id && id < 85) {
                ejMultiplicity++;
              } else if (86 < id && id < 112) {
                gsMultiplicity++;
              }
            }
          }

          if (fillingFlag && multiplicity > 1) {
            std::sort(event->begin(), event->end(),
                      [](const THitClass &a, const THitClass &b) {
                        return a.Timestamp < b.Timestamp;
                      });

            if (multiplicity > 2 && eneSum > 1000 && gammaMultiplicity > 0)
              isFissionTrigger = true;

            // if (isFissionTrigger) tree->Fill();
            tree->Fill();
          }

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