#ifndef RENDERING_SHADER_PIPELINE_HPP
#define RENDERING_SHADER_PIPELINE_HPP

#include <Graphics/Shading/Shader.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

namespace ettycc
{
    class ShaderPipeline
    {
    private:
        GLuint shaderProgram = 0;
        std::vector<std::shared_ptr<Shader>> shaders_;

    public:
        ShaderPipeline();
        ShaderPipeline(const std::vector<Shader>& shaders);
        ~ShaderPipeline();
        
        // SHADER PIPELINE API 
        void Create();
        int Bind();
        int Unbind();

        GLuint GetProgramId() const;

        void AddShaders(const std::vector<std::shared_ptr<Shader>>& shaders);
        void AddShadersFromFiles(const std::vector<std::pair<std::string, GLenum>> &shaderConfigs);
    };
}

#endif