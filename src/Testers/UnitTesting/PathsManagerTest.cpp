#include <gtest/gtest.h>
#include <80CC.hpp>
#include <memory>
#include <spdlog/spdlog.h>
using namespace ettycc;

std::shared_ptr<PathManager> pathManager;

class PathsManangerTestFixture : public testing::Test
{
protected:
    void SetUp() override
    {
        // Dependency registration
        pathManager = std::make_shared<PathManager>();

        RegisterDependency(PathManager, pathManager);
    }

    void TearDown() override
    {
     
    }
};

TEST_F(PathsManangerTestFixture, json_config_loader)
{
    auto paths = GetDependency(PathManager);
    paths->AddPath("key1", "path1");
    paths->AddPath("key2", "path2");
    paths->SaveToJson("paths.json");

    paths->LoadFromJson("paths.json");

    spdlog::info("Loaded path for key1: {}", paths->GetPath("key1"));
    spdlog::info("Loaded path for key2: {}", paths->GetPath("key2"));

    EXPECT_TRUE(paths->GetPath("key1").compare("path1") == 0);
    EXPECT_TRUE(paths->GetPath("key2").compare("path2") == 0);
}