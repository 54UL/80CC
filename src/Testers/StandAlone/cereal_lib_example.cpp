#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
// #include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

#include <fstream>
    
struct MyRecord
{
  uint8_t x, y;
  float z;
  
  template <class Archive>
  void serialize( Archive & ar )
  {
    ar( x, y, z );
  }
};
    
struct SomeData
{
  int32_t id;
  std::shared_ptr<std::unordered_map<uint32_t, MyRecord>> data;
  
  template <class Archive>
  void save( Archive & ar ) const
  {
    ar( data );
  }
      
  template <class Archive>
  void load( Archive & ar )
  {
    static int32_t idGen = 0;
    id = idGen++;
    ar( data );
  }
};

int main()
{
  // std::ofstream os("out.json", std::ios::binary); // USE THIS TO DUMP THE ARCHIVE INTO  A FILE
  std::stringstream ss;

//   cereal::BinaryOutputArchive archive( os ); // USE THIS TO DUMP THE ARCHIVE INTO  A FILE
  cereal::JSONOutputArchive archive(ss);

  SomeData myData;
  SomeData myData2;

  archive( myData, myData2);
  spdlog::info("JSON VALUE {}",ss.str());
  return 0;
}