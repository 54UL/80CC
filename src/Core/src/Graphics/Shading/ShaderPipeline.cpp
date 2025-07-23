#include <Graphics/Shading/ShaderPipeline.hpp>
#include <Graphics/Shading/Shader.hpp>
#include <vector>
#include <spdlog/spdlog.h>


namespace ettycc
{
    ShaderPipeline::ShaderPipeline()
    {
    }

    ShaderPipeline::ShaderPipeline(const std::vector<Shader> &shaders)
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

            // std::string shaderSource((std::istreambuf_iterator<char>(shaderFile)));
            std::string shaderSource= "NULL";
            // Create a Shader object and add it to the shaders vector
            shaders_.emplace_back(std::make_shared<Shader>(shaderSource, shaderConfig.second));
        }
    }

    void ShaderPipeline::AddShaders(const std::vector<std::shared_ptr<Shader>> &shaders)
    {
        shaders_.insert(shaders_.end(), shaders.begin(), shaders.end());
    }

    void ShaderPipeline::Create()
    {
        shaderProgram = glCreateProgram();

        for (auto shader : shaders_)
        {
            glAttachShader(shaderProgram, shader->GetShaderId());
        }

        glLinkProgram(shaderProgram);

        GLint success;
        char * infoLog;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            spdlog::error("Shader program linking failed:\n", infoLog);
        }
        
        glLinkProgram(0);
    }

    int ShaderPipeline::Bind()
    {
        if (shaderProgram == 0)
        {
            spdlog::error("Binding shader pipeline with null shader program id (itwas 0)");
            return 1;
        }
        glUseProgram(shaderProgram);

        return 0;
    }

    int ShaderPipeline::Unbind()
    {
        if (shaderProgram == 0)
        {
            return 1;
        }
        glUseProgram(shaderProgram);

        return 0;
    }

    GLuint ShaderPipeline::GetProgramId() const
    {
        return shaderProgram;
    }
} // namespace ettycc
