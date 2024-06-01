#ifndef TChSettings_hpp
#define TChSettings_hpp 1

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class TChSettings
{
 public:
  TChSettings(){};
  ~TChSettings(){};

  uint32_t mod = 0;
  uint32_t ch = 0;
  uint32_t ACMod = 0;
  uint32_t ACCh = 0;
  bool isPMT = false;
  bool isEventTrigger = false;
  bool hasAC = false;

  void Print()
  {
    std::cout << "Module: " << mod << "\tChannel: " << ch << std::endl;
    std::cout << "\tIsEventTrigger: " << isEventTrigger << std::endl;
    std::cout << "\tACModule: " << ACMod << std::endl;
    std::cout << "\tACChannel: " << ACCh << std::endl;
    std::cout << "\tisPMT: " << isPMT << std::endl;
    std::cout << "\thasAC: " << hasAC << std::endl;
    std::cout << std::endl;
  };

  static void GenerateTemplate(uint32_t nMods = 10, uint32_t nChs = 16)
  {
    nlohmann::json result;
    for (auto i = 0; i < nMods; i++) {
      nlohmann::json mod;
      for (auto j = 0; j < nChs; j++) {
        nlohmann::json ch;
        ch["Module"] = i;
        ch["Channel"] = j;
        ch["ACModule"] = i;
        ch["ACChannel"] = j;
        ch["IsPMT"] = false;
        ch["IsEventTrigger"] = false;
        ch["HasAC"] = false;
        mod.push_back(ch);
      }
      result.push_back(mod);
    }

    std::ofstream ofs("chSettings.json");
    ofs << result.dump(4) << std::endl;
    ofs.close();
  };
};

typedef TChSettings ChSettings_t;
typedef std::vector<std::vector<ChSettings_t>> ChSettingsVec_t;

#endif