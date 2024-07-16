#include "THitLoader.hpp"

#include <TFile.h>
#include <TKey.h>
#include <TTree.h>

#include <iostream>
#include <mutex>
#include <parallel/algorithm>
#include <thread>

std::unique_ptr<std::vector<THitData>> THitLoader::LoadHitsMT(
    std::vector<std::string> fileList, HitFileType fileType)
{
  fHitVec = std::make_unique<std::vector<THitData>>();
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

  std::vector<std::thread> threads;
  for (auto i = 0; i < fileList.size(); i++) {
    std::cout << "Loading file: " << fileList[i] << std::endl;
    threads.emplace_back([this, i, fileList, fileType]() {
      switch (fileType) {
        case HitFileType::DELILA:
          LoadDELILAHits(fileList[i]);
          break;
        case HitFileType::ELIGANT:
          LoadELIGANTHits(fileList[i]);
          break;
        default:
          std::cerr << "Unknown file type" << std::endl;
          break;
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  std::cout << "Sorting hits" << std::endl;
  __gnu_parallel::sort(fHitVec->begin(), fHitVec->end(),
                       [](const THitData &a, const THitData &b) {
                         return a.Timestamp < b.Timestamp;
                       });

  return std::move(fHitVec);
}

void THitLoader::LoadDELILAHits(const std::string &fileName)
{
  ROOT::EnableThreadSafety();
}

void THitLoader::LoadELIGANTHits(const std::string &fileName)
{
  ROOT::EnableThreadSafety();

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

  auto hitsVec = std::make_unique<std::vector<THitData>>();
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
  fHitVec->insert(fHitVec->end(), std::make_move_iterator(hitsVec->begin()),
                  std::make_move_iterator(hitsVec->end()));
}