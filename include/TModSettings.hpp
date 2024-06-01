#ifndef TModSettings_hpp
#define TModSettings_hpp 1

// data class of module settings
// Loading modSettings.json and store the settings

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

class TModSettings
{
 public:
  TModSettings(){};
  ~TModSettings(){};

  uint32_t mod;
  uint32_t nChs;
  std::string FW;
  double pulserDelay;  // delay of pulser singal, not for the analysis
  double timeOffset;   // time difference between modules
  uint32_t pulserCh;

  void Print()
  {
    std::cout << "Module: " << mod << std::endl;
    std::cout << "\tNumber of channels: " << nChs << std::endl;
    std::cout << "\tFW: " << FW << std::endl;
    std::cout << "\tPulser delay: " << pulserDelay << std::endl;
    std::cout << "\tTime offset: " << timeOffset << std::endl;
    std::cout << "\tPulser channel: " << pulserCh << std::endl;
    std::cout << std::endl;
  };

  static void GenerateTemplate(uint32_t nMods = 10)
  {
    nlohmann::json result;
    nlohmann::json mod;
    for (auto i = 0; i < nMods; i++) {
      mod["Module"] = i;
      mod["NChannels"] = 16;
      mod["FW"] = "PHA";
      mod["PulserDelay"] = 0.0;
      mod["TimeOffset"] = 0.0;
      mod["PulserCh"] = 0;
      result.push_back(mod);
    }

    std::ofstream ofs("modSettings.json");
    ofs << result.dump(4) << std::endl;
    ofs.close();
  };
};

typedef TModSettings ModSettings_t;
typedef std::vector<ModSettings_t> ModSettingsVec_t;

#endif