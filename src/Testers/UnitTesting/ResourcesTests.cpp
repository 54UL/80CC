#include <gtest/gtest.h>
#include <80CC.hpp>

#include <spdlog/spdlog.h>

using namespace ettycc;

#include <string>


#include <cereal/types/memory.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/archives/json.hpp>

const char* configFileName = "80CC.json";
Resources resources;

// Fixture for Resources class
class ResourcesTest : public ::testing::Test
{
protected:

    static void SetUpTestCase()
    {
        // TODO: THIS PICE OF CODE NEEDS TO BE INITIALIZED ALONG WITH THE ENGINE
        const char* engineWorkingFolder = std::getenv("ASSETS_80CC");
        if (engineWorkingFolder == nullptr) 
        {
            spdlog::warn("Engine working folder not set... using [{}]", paths::CONFIG_DEFAULT);    
            resources.SetWorkingFolder(paths::CONFIG_DEFAULT);
        }
        else 
        {
            // Assuming the proyect structure is: 80cc/build/Testers
            spdlog::info("Engine working folder '{}'", engineWorkingFolder);
            resources.SetWorkingFolder(std::string(engineWorkingFolder) + "/config/");
        }
    }

    void TearDown() override
    {
        // Clean up resources after each test
    }
};

// This test generates the base configuration file 
TEST_F(ResourcesTest, engine_resource_file_generation)
{
    // Store a resource
    resources.Set("app", "title", "80CC");
    resources.Set("app", "flags", "WINDOWED");
    resources.Set("app", "resolution", "800,600");

    resources.Set("paths", "config", "config/");
    resources.Set("paths", "images", "images/");
    resources.Set("paths", "scenes", "scenes/");
    resources.Set("paths", "shaders", "shaders/");
    resources.Set("paths", "templates", "templates/");

    resources.Set("sprites", "loona", "images/loona.jpg");
    resources.Set("sprites", "not-found", "images/not_found_texture.png");

    resources.Set("shaders", "sprite_shader", "shaders/main");
    resources.Set("state", "last_scene", "default_scene.json");

    resources.Store(configFileName);
}

TEST_F(ResourcesTest, engine_load_resource)
{
     // Load the stored resources using the default asset path for development...
    Resources newResources;
    
    newResources.SetWorkingFolder(paths::CONFIG_DEFAULT);
    newResources.Load(configFileName);

    EXPECT_TRUE(newResources.Get("sprites", "loona").compare("images/loona.jpg") == 0);
}


