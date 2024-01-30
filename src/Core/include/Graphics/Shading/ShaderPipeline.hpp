#ifndef RENDERING_SHADER_PIPELINE_HPP
#define RENDERING_SHADER_PIPELINE_HPP

#include "Shader.hpp"
#include <fstream>
#include <vector>
#include <memory>

namespace etycc
{
    class ShaderPipeline
    {
    private:
        GLuint shaderProgram = 0;
        std::vector<std::shared_ptr<Shader>> shaders_;

    public:
        ShaderPipeline(const std::vector<Shader>& shaders);
        ~ShaderPipeline();
        
        // SHADER PIPELINE API 
        void Create();
        void Use();
        GLuint GetProgramId() const;

        void AddShaders(const std::vector<std::shared_ptr<Shader>>& shaders);
        void AddShadersFromFiles(const std::vector<std::pair<std::string, GLenum>> &shaderConfigs);
    };
}

#endif