#ifndef THitLoader_HPP
#define THitLoader_HPP 1

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include "TChSettings.hpp"
#include "THitData.hpp"

enum class HitFileType { DELILA, ELIGANT };

class THitLoader
{
 public:
  THitLoader(){};
  THitLoader(ChSettingsVec_t chSettingsVec) : fChSettingsVec(chSettingsVec){};
  ~THitLoader(){};

  std::unique_ptr<std::vector<HitData_t>> LoadHitsMT(
      std::vector<std::string> fileList, uint32_t nThreads,
      HitFileType fileType = HitFileType::DELILA);

 private:
  ChSettingsVec_t fChSettingsVec;

  std::unique_ptr<std::vector<HitData_t>> fHitVec;
  std::vector<bool> fInsertFlags;
  std::mutex fHitVecMutex;
  std::mutex fFileListMutex;
  void LoadDELILAHits(std::string fileName, uint32_t threadID);
  void LoadELIGANTHits(std::string fileName, uint32_t threadID);
};

#endif