#include "TCoincidenceCondition.hpp"

bool TCoincidenceCondition::CheckCoincidence(std::vector<THitData> hitVec)
{
  uint32_t nHits = hitVec.size();
  if (nHits == 0) {
    return false;
  }

  uint32_t nCoincidence = 0;
  for (const auto &hit : hitVec) {
    auto id = fChSettings[hit.Board][hit.Channel].coincidenceID;
    if (id == this->fID) {
      nCoincidence++;
    }
  }

  bool result = false;
  if (fResultType == "Accept") {
    result = true;
  }

  if (fConditionType == "MoreThan") {
    if (nCoincidence > fThreshold) {
      return result;
    }
  } else if (fConditionType == "LessThan") {
    if (nCoincidence < fThreshold) {
      return result;
    }
  } else if (fConditionType == "EqualTo") {
    if (nCoincidence == fThreshold) {
      return result;
    }
  }

  return !result;
}