#include <TFile.h>
#include <TSystem.h>
#include <TTree.h>

#include <iostream>
#include <vector>

#include "DELILAHit.hpp"

void reader()
{
  gSystem->Load("libEveBuilder.so");

  auto file = TFile::Open("output.root");
  if (!file) {
    std::cerr << "File not found: output.root" << std::endl;
    return;
  }
  auto tree = dynamic_cast<TTree *>(file->Get("tree"));

  std::vector<DELILAHit> *hits = nullptr;
  tree->SetBranchAddress("event", &hits);

  std::cout << "Entries: " << tree->GetEntries() << std::endl;

  file->Close();
}