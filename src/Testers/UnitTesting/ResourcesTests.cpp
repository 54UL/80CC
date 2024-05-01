#include <gtest/gtest.h>
#include <80CC.hpp>

#include <spdlog/spdlog.h>

using namespace ettycc;

#include <cereal/types/memory.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/archives/json.hpp>

// Fixture for Resources class
class ResourcesTest : public ::testing::Test
{
protected:
    Resources resources;

    void SetUp() override
    {
        // Load some test resources before each test
        // resources.Load("test_resources.json");
    }

    void TearDown() override
    {
        // Clean up resources after each test
    }
};

TEST_F(ResourcesTest, StoreAndLoadResource)
{
    // Store a resource
    resources.Set("test_resources.json", "key2", "value2");
    resources.Set("test_resources2.json", "key2", "value2");

    resources.Store("test_resources.json");

    // Load the stored resource
    Resources newResources;
    newResources.Load("test_resources.json");
    ASSERT_EQ("value2", newResources.Get("test_resources.json", "key2"));
}

TEST_F(ResourcesTest, LoadAndGetResource)
{
    // Verify that a resource is loaded and can be retrieved
    ASSERT_EQ("value1", resources.Get("test_resources.json", "key1"));
}
