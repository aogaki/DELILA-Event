#ifndef TELIGANTSettings_hpp
#define TELIGANTSettings_hpp 1

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class TELIGANTSettings
{
 public:
  TELIGANTSettings(){};
  ~TELIGANTSettings(){};

  uint32_t mod = 0;
  uint32_t ch = 0;
  double timeOffset = 0.;
  bool isEventTrigger = false;
};

typedef TELIGANTSettings ELIGANTSettings_t;
typedef std::vector<std::vector<ELIGANTSettings_t>> ELIGANTSettingsVec_t;

#endif