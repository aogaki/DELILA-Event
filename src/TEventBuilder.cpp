#include "TEventBuilder.hpp"

#include <TROOT.h>
#include <unistd.h>

#include <parallel/algorithm>

TEventBuilder::TEventBuilder(Double_t timeWindow, ChSettingsVec_t chSettingsVec,
                             std::vector<std::string> fileList,
                             HitFileType hitType)
{
  fTimeWindow = timeWindow;
  fChSettingsVec = chSettingsVec;
  fFileList = fileList;
  fHitType = hitType;
}

void TEventBuilder::BuildEvent(uint32_t nFiles, uint32_t nThreads)
{
  auto hitLoader = THitLoader(fChSettingsVec);

  bool firstRun = true;
  while (true) {
    if (fFileList.size() == 0) {
      break;
    }

    std::vector<std::string> fileList;
    for (auto i = 0; i < nFiles; i++) {
      if (fFileList.size() == 0) {
        break;
      }
      fileList.push_back(fFileList.front());
      fFileList.erase(fFileList.begin());
    }
    fHitVec = hitLoader.LoadHitsMT(fileList, nThreads, fHitType);
    std::cout << fHitVec->size() << " hits loaded" << std::endl;

    if (fHitVec->size() == 0) {
      continue;
    } else {
    }

    if (fHitType == HitFileType::ELIGANT) {
      SearchAndWriteELIGANTEvents(nThreads, firstRun);
    }

    fHitVec.reset();
    firstRun = false;
  }
}

Double_t TEventBuilder::GetCalibratedEnergy(const ChSettings_t &chSetting,
                                            const UShort_t &adc)
{
  return chSetting.p0 + chSetting.p1 * adc + chSetting.p2 * adc * adc +
         chSetting.p3 * adc * adc * adc;
}

void TEventBuilder::SearchAndWriteELIGANTEvents(uint32_t nThreads,
                                                bool firstRun)
{
  // ROOT::EnableThreadSafety();

  std::vector<std::thread> threads;
  for (auto i = 0; i < nThreads; i++) {
    threads.emplace_back([this, i, nThreads, firstRun]() {
      auto fileName = Form("event_t%d.root", i);
      auto treeName = Form("Event_Tree");
      TFile *file = nullptr;
      TTree *tree = nullptr;
      std::vector<THitData> *event = new std::vector<THitData>();
      UChar_t triggerID;
      Double_t triggerTS;
      UChar_t multiplicity;
      UChar_t gammaMultiplicity;
      UChar_t ejMultiplicity;
      UChar_t gsMultiplicity;
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

      for (auto j = i; j < fHitVec->size(); j += nThreads) {
        auto hit = THitData(fHitVec->at(j));
        if (fChSettingsVec.at(hit.Board).at(hit.Channel).isEventTrigger) {
          bool fillingFlag = true;

          triggerID = fChSettingsVec.at(hit.Board).at(hit.Channel).detectorID;
          triggerTS = hit.Timestamp;
          multiplicity = 0;
          gammaMultiplicity = 0;
          ejMultiplicity = 0;
          gsMultiplicity = 0;
          isFissionTrigger = false;

          double eneSum = GetCalibratedEnergy(
              fChSettingsVec.at(hit.Board).at(hit.Channel), hit.Energy);

          const Double_t eventTS = triggerTS;
          event->emplace_back(hit.Board, hit.Channel, 0, hit.Energy,
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
              auto hitPast = THitData(fHitVec->at(k));
              if (hitPast.Timestamp < eventTS - fTimeWindow / 2) {
                break;
              }
              int32_t detectorID = fChSettingsVec.at(hitPast.Board)
                                       .at(hitPast.Channel)
                                       .detectorID;
              if (0 <= detectorID && detectorID < triggerID) {
                fillingFlag = false;
                break;
              }

              event->emplace_back(hitPast.Board, hitPast.Channel,
                                  hitPast.Timestamp - eventTS, hitPast.Energy,
                                  hitPast.EnergyShort);

              eneSum += GetCalibratedEnergy(
                  fChSettingsVec.at(hitPast.Board).at(hitPast.Channel),
                  hitPast.Energy);

              auto id = hitPast.Board * 16 + hitPast.Channel;
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
          if (fillingFlag && j + 1 < fHitVec->size()) {
            for (auto k = j + 1; k < fHitVec->size(); k++) {
              auto hitFuture = THitData(fHitVec->at(k));
              if (hitFuture.Timestamp > eventTS + fTimeWindow / 2) {
                break;
              }
              int32_t detectorID = fChSettingsVec.at(hitFuture.Board)
                                       .at(hitFuture.Channel)
                                       .detectorID;
              if (0 <= detectorID && detectorID < triggerID) {
                fillingFlag = false;
                break;
              }

              event->emplace_back(hitFuture.Board, hitFuture.Channel,
                                  hitFuture.Timestamp - eventTS,
                                  hitFuture.Energy, hitFuture.EnergyShort);

              eneSum += GetCalibratedEnergy(
                  fChSettingsVec.at(hitFuture.Board).at(hitFuture.Channel),
                  hitFuture.Energy);

              auto id = hitFuture.Board * 16 + hitFuture.Channel;
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

          if (fillingFlag && multiplicity > 1) {  // reject single hit event
            std::sort(event->begin(), event->end(),
                      [](const THitData &a, const THitData &b) {
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

  fHitVec->clear();
  // fHitVec->reset();
}