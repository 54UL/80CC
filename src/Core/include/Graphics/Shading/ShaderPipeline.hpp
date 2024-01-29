#ifndef RENDERING_SHADER_PIPELINE_HPP
#define RENDERING_SHADER_PIPELINE_HPP

#include "Shader.hpp"
#include <fstream>
#include <vector>

namespace etycc
{
    class ShaderPipeline
    {
    private:
        GLuint shaderProgram = 0;

    public:
        ShaderPipeline(const std::vector<Shader>& shaders);
        ~ShaderPipeline();

        // SHADER API 
        GLuint GetProgramId() const;
        void Use();
    };
}

#endif