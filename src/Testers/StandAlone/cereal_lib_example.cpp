#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>

// #include <cereal/archives/binary.hpp>

#include <spdlog/spdlog.h>
#include <sstream>
#include <fstream>
#include <stdint.h>

struct MyRecord
{
  uint8_t x, y;
  float z;
  // const char * lol = "xdxd";

  template <class Archive>
  void serialize(Archive &ar)
  {
    ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(z));
  }
};

int main()
{
  // std::ofstream os("out.json", std::ios::binary); // USE THIS TO DUMP THE ARCHIVE INTO  A FILE
  std::stringstream ss;
  // cereal::BinaryOutputArchive archive( os ); // USE THIS TO DUMP THE ARCHIVE INTO  A FILE

  { 
    // IMPORTANT USE RAII OR THIS WON'T WORK !!
    cereal::JSONOutputArchive archive(ss);

    MyRecord exampleData;
    exampleData.x = 1;
    exampleData.y = 0xff;
    exampleData.z = 0.033f;

    archive(CEREAL_NVP(exampleData));
    spdlog::info("JSON VALUE {}", ss.str());
  }

  {
    MyRecord newData;
    cereal::JSONInputArchive archive2(ss);
    archive2(newData);
    spdlog::info("DESERIALIZED VALUE x:{} y:{} z:{}", newData.x, newData.y, newData.z);
  }

  return 0;
}