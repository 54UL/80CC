#include <Shading/ShaderPipeline.hpp>
#include <Shading/Shader.hpp>

#include <vector>
#include "ShaderPipeline.hpp"

namespace ettycc
{
    ShaderPipeline::ShaderPipeline()
    {
    }

    ShaderPipeline::~ShaderPipeline()
    {
    }

    void ShaderPipeline::AddShadersFromFiles(const std::vector<std::pair<std::string, GLenum>> &shaderConfigs)
    {
        std::cout << "Adding shaders..." << std::endl;
        for (const auto &shaderConfig : shaderConfigs)
        {
            std::ifstream shaderFile(shaderConfig.first);
            if (!shaderFile.is_open())
            {
                std::cerr << "Failed to open shader file: " << shaderConfig.first << std::endl;
                continue;
            }

            std::string shaderSource((std::istreambuf_iterator<char>(shaderFile)));

            // Create a Shader object and add it to the shaders vector
            shaders_.emplace_back(std::make_shared<Shader>(shaderSource, shaderConfig.second));
        }
    }

    void AddShaders(const std::vector<std::shared_ptr<Shader>> &shaders)
    {
        shaders_.insert(shaders_.begin(), shaders.begin(), shaders.end());
    }

    void Create()
    {
        // Create program
        shaderProgram = glCreateProgram();

        // Attach shaders...
        for (auto shader : shaders_)
        {
            glAttachShader(shaderProgram, shader->GetShaderId);
        }

        // Create and link the shader program
        glLinkProgram(shaderProgram);

        // Check for shader program linking errors
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            std::cerr << "Shader program linking failed:\n"
                      << infoLog << std::endl;
        }
    }

    void ShaderPipeline::Use()
    {
        glUseProgram(shaderProgram);
    }

    GLuint ShaderPipeline::GetProgramId() const
    {
        return shaderProgram;
    }
} // namespace ettycc
