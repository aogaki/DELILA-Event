#include "THitLoader.hpp"

#include <TFile.h>
#include <TKey.h>
#include <TROOT.h>
#include <TTree.h>
#include <unistd.h>

#include <execution>
#include <iostream>
#include <mutex>
#include <parallel/algorithm>
#include <thread>

std::unique_ptr<std::vector<HitData_t>> THitLoader::LoadHitsMT(
    std::vector<std::string> fileList, uint32_t nThreads, HitFileType fileType)
{
  fHitVec = std::make_unique<std::vector<HitData_t>>();
  auto nHits = 0;
  for (auto &fileName : fileList) {
    auto file = new TFile(fileName.c_str(), "READ");
    TIter nextkey(file->GetListOfKeys());
    while (auto key = (TKey *)nextkey()) {
      TObject *obj = key->ReadObj();
      if (obj->IsA()->InheritsFrom(TTree::Class())) {
        auto tree = (TTree *)obj;
        nHits += tree->GetEntries();
        file->Close();
        delete file;
        break;
      }
    }
  }
  fHitVec->reserve(nHits);

  while (true) {
    if (fileList.size() == 0) {
      break;
    }

    fInsertFlags.clear();
    for (auto i = 0; i < nThreads; i++) {
      fInsertFlags.push_back(false);
    }
    fInsertFlags[0] = true;

    std::vector<std::string> threadFileList;
    for (auto i = 0; i < nThreads; i++) {
      if (fileList.size() == 0) {
        break;
      }
      threadFileList.push_back(fileList.front());
      fileList.erase(fileList.begin());
    }

    std::vector<std::thread> threads;
    for (auto i = 0; i < threadFileList.size(); i++) {
      threads.emplace_back([this, i, threadFileList, fileType]() {
        switch (fileType) {
          case HitFileType::DELILA:
            LoadDELILAHits(threadFileList[i], i);
            break;
          case HitFileType::ELIGANT:
            LoadELIGANTHits(threadFileList[i], i);
            break;
          default:
            std::cerr << "Unknown file type" << std::endl;
            break;
        }
      });

      usleep(100000);
    }

    for (auto &thread : threads) {
      thread.join();
    }
  }

  std::cout << "Sorting hits" << std::endl;
  __gnu_parallel::sort(fHitVec->begin(), fHitVec->end(),
                       [](const HitData_t &a, const HitData_t &b) {
                         return std::get<2>(a) < std::get<2>(b);
                       });
  // std::sort(std::execution::par, fHitVec->begin(), fHitVec->end(),
  //           [](const HitData_t &a, const HitData_t &b) {
  //             return std::get<2>(a) < std::get<2>(b);
  //           });

  return std::move(fHitVec);
}

void THitLoader::LoadDELILAHits(std::string fileName, uint32_t threadID)
{
  ROOT::EnableThreadSafety();
  {
    std::lock_guard<std::mutex> lock(fFileListMutex);
    std::cout << "Loading hits from " << fileName << std::endl;
  }
  auto file = TFile::Open(fileName.c_str(), "READ");
  if (!file) {
    std::cerr << "File not found: " << fileName << std::endl;
    return;
  }
  auto tree = dynamic_cast<TTree *>(file->Get("ELIADE_Tree"));

  tree->SetBranchStatus("*", kFALSE);
  UChar_t brd;
  tree->SetBranchStatus("Mod", kTRUE);
  tree->SetBranchAddress("Mod", &brd);
  UChar_t ch;
  tree->SetBranchStatus("Ch", kTRUE);
  tree->SetBranchAddress("Ch", &ch);

  UShort_t ene;
  tree->SetBranchStatus("ChargeLong", kTRUE);
  tree->SetBranchAddress("ChargeLong", &ene);

  UShort_t eneShort;
  tree->SetBranchStatus("ChargeShort", kTRUE);
  tree->SetBranchAddress("ChargeShort", &eneShort);

  Double_t ts;
  tree->SetBranchStatus("FineTS", kTRUE);
  tree->SetBranchAddress("FineTS", &ts);

  auto hitsVec = std::vector<HitData_t>();
  hitsVec.reserve(tree->GetEntries());
  for (auto i = 0; i < tree->GetEntries(); i++) {
    tree->GetEntry(i);
    auto fineTS = ts / 1000. + fChSettingsVec.at(brd).at(ch).timeOffset;

    hitsVec.emplace_back(brd, ch, fineTS, ene, eneShort);

    // std::cout << "brd: " << brd << " ch: " << ch << " ts: " << fineTS
    //           << " ene: " << ene << " eneShort: " << eneShort << std::endl;
  }

  file->Close();
  while (true) {
    if (fInsertFlags[threadID]) {
      break;
    }
    usleep(1000);
  }
  {
    std::lock_guard<std::mutex> lock(fHitVecMutex);
    fHitVec->insert(fHitVec->end(), hitsVec.begin(), hitsVec.end());
    if (threadID + 1 < fInsertFlags.size()) fInsertFlags[threadID + 1] = true;
    std::cout << "Finished: " << fileName << std::endl;
  }
  hitsVec.clear();
}

void THitLoader::LoadELIGANTHits(std::string fileName, uint32_t threadID)
{
  ROOT::EnableThreadSafety();
  {
    std::lock_guard<std::mutex> lock(fFileListMutex);
    std::cout << "Loading hits from " << fileName << std::endl;
  }
  auto file = TFile::Open(fileName.c_str(), "READ");
  if (!file) {
    std::cerr << "File not found: " << fileName << std::endl;
    return;
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

  auto hitsVec = std::vector<HitData_t>();
  hitsVec.reserve(tree->GetEntries());
  for (auto i = 0; i < tree->GetEntries(); i++) {
    tree->GetEntry(i);
    if (flag == 0) continue;
    Double_t fineTS =
        Double_t(ts) / 1000. + fChSettingsVec.at(brd).at(ch).timeOffset;
    hitsVec.emplace_back(brd, ch, fineTS, ene, eneShort);

    // std::cout << "brd: " << brd << " ch: " << ch << " ts: " << fineTS
    //           << " ene: " << ene << " eneShort: " << eneShort << std::endl;
  }

  file->Close();
  while (true) {
    if (fInsertFlags[threadID]) {
      break;
    }
    usleep(100);
  }
  {
    std::lock_guard<std::mutex> lock(fHitVecMutex);
    fHitVec->insert(fHitVec->end(), hitsVec.begin(), hitsVec.end());
    if (threadID + 1 < fInsertFlags.size()) fInsertFlags[threadID + 1] = true;
  }
  hitsVec.clear();
}
