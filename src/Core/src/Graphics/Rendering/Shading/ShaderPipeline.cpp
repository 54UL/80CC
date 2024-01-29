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

    void Create(const std::vector<std::shared_ptr<Shader>>& shaders){
        // Create program
        shaderProgram = glCreateProgram();

        // Attach shaders...
        for (auto shader : shaders){
            glAttachShader(shaderProgram, shader->GetShaderId);
        }
     
        // Create and link the shader program
        glLinkProgram(shaderProgram);

        // Check for shader program linking errors
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
        }
    }

    GLuint ShaderPipeline::GetProgramId() const
    {
        return shaderProgram;
    }

    void ShaderPipeline::Use()
    {
        glUseProgram(shaderProgram);
    }

} // namespace ettycc

