#ifndef TCoincidenceCondition_hpp
#define TCoincidenceCondition_hpp 1

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "TChSettings.hpp"
#include "THitData.hpp"

class TCoincidenceCondition
{
 public:
  TCoincidenceCondition(){};
  TCoincidenceCondition(std::string resultType, std::string conditionType,
                        uint32_t id, uint32_t threshold,
                        ChSettingsVec_t chSettings)
      : fResultType(resultType),
        fConditionType(conditionType),
        fID(id),
        fThreshold(threshold),
        fChSettings(chSettings){};
  ~TCoincidenceCondition(){};

  bool CheckCoincidence(std::vector<THitData> hitVec);

 private:
  std::string fResultType = "Accept";
  std::string fConditionType = "MoreThan";
  uint32_t fID = 0;
  uint32_t fThreshold = 0;
  ChSettingsVec_t fChSettings;
};

#endif
