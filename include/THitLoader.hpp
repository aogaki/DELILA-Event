#ifndef THitLoader_HPP
#define THitLoader_HPP 1

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
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

  std::unique_ptr<std::vector<THitData>> LoadHitsMT(
      std::vector<std::string> fileList,
      HitFileType fileType = HitFileType::DELILA);

 private:
  ChSettingsVec_t fChSettingsVec;

  std::unique_ptr<std::vector<THitData>> fHitVec;
  std::mutex fHitVecMutex;
  void LoadDELILAHits(const std::string &fileName);
  void LoadELIGANTHits(const std::string &fileName);
};

#endif