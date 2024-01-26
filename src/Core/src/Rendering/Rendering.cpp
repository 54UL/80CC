#include <Rendering.hpp>

namespace ettycc
{
    Rendering::Rendering(){

    }

    Rendering::~Rendering(){
        
    }

    void Rendering::AddShaders(const std::vector<std::pair<std::string, GLenum>> &shaderConfigs)
    {
        // Add shaders to the rendering system
        std::cout << "Adding shaders..." << std::endl;
        for (const auto &shaderConfig : shaderConfigs)
        {
            // Load shader source from file (you can modify this part)
            std::ifstream shaderFile(shaderConfig.first);
            if (!shaderFile.is_open())
            {
                std::cerr << "Failed to open shader file: " << shaderConfig.first << std::endl;
                continue;
            }

            std::string shaderSource((std::istreambuf_iterator<char>(shaderFile)),
                                     std::istreambuf_iterator<char>());

            // Create a Shader object and add it to the shaders vector
            shaders_.emplace_back(std::make_shared<Shader>(shaderSource, shaderConfig.second));
        }
    }
}
