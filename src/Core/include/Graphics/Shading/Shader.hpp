#ifndef RENDERING_SHADER_HPP
#define RENDERING_SHADER_HPP

// rendering.cpp
#include <fstream>
#include <gl/GL.h>

namespace etycc
{
    class Shader
    {
    private:
        std::string sourceCode_;
        GLenum type_;
        GLuint shaderId_ = 0;

    public:
        Shader(const std::string& sourceCode, GLenum type);
        ~Shader();
        // SHADER API
        GLuint GetShaderId() const;
    private:
        void Compile();
    };
}

#endif