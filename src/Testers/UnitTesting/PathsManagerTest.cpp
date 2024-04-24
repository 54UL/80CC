#include <gtest/gtest.h>
#include <80CC.hpp>
#include <memory>
#include <spdlog/spdlog.h>
using namespace ettycc;

std::shared_ptr<FilePersist> filePersistManager;

class PathsManangerTestFixture : public testing::Test
{
protected:
    void SetUp() override
    {
        // Dependency registration
        filePersistManager = std::make_shared<FilePersist>();

        RegisterDependency(FilePersist, filePersistManager);
    }

    void TearDown() override
    {
     
    }
};

TEST_F(PathsManangerTestFixture, json_config_loader)
{
    auto configFile = GetDependency(FilePersist);
    configFile->AddValue("key1", "path1");
    configFile->AddValue("key2", "path2");

    configFile->SaveToJson("paths.json");
    configFile->LoadFromJson("paths.json");

    spdlog::info("Loaded path for key1: {}", configFile->GetValue("key1"));
    spdlog::info("Loaded path for key2: {}", configFile->GetValue("key2"));

    EXPECT_TRUE(configFile->GetValue("key1").compare("path1") == 0);
    EXPECT_TRUE(configFile->GetValue("key2").compare("path2") == 0);
}