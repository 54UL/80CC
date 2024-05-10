#include <gtest/gtest.h>
#include <80CC.hpp>

#include <spdlog/spdlog.h>

using namespace ettycc;

#include <string>


#include <cereal/types/memory.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/archives/json.hpp>

const std::string engineConfigPath = "80CC.json";
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
            spdlog::warn("Engine working folder not set... using '../../assets'");    
            resources.SetWorkingFolder(std::string("../../assets") + "/config/");
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

    resources.Set("paths", "config", "assets/config/");
    resources.Set("paths", "images", "assets/images/");
    resources.Set("paths", "scenes", "assets/scenes/");
    resources.Set("paths", "shaders", "assets/shaders/");
    resources.Set("paths", "templates", "assets/templates/");

    resources.Set("sprites", "loona", "assets/images/loona.jpg");
    resources.Set("sprites", "not-found", "assets/images/not_found_texture.png");

    resources.Set("shaders", "sprite_shader", "assets/shaders/main");
    resources.Set("state", "last_scene", "default_scene.json");

    resources.Store(engineConfigPath);
}

TEST_F(ResourcesTest, engine_load_resource)
{
     // Load the stored resources using the default asset path for development...
    Resources newResources;
    
    newResources.SetWorkingFolder(std::string("../../assets") + "/config/");
    newResources.Load(engineConfigPath);

    EXPECT_TRUE(newResources.Get("sprites", "loona").compare("assets/images/loona.jpg") == 0);
}


