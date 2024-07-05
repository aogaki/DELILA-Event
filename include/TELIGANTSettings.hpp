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
  double_t timeOffset = 0.;
  bool isEventTrigger = false;

  double_t phi = 0.;
  double_t theta = 0.;

  double_t x = 0.;
  double_t y = 0.;
  double_t z = 0.;
  double_t r = 0.;

  double_t p0 = 0.;
  double_t p1 = 1.;
  double_t p2 = 0.;
  double_t p3 = 0.;
};

typedef TELIGANTSettings ELIGANTSettings_t;
typedef std::vector<std::vector<ELIGANTSettings_t>> ELIGANTSettingsVec_t;

#endif