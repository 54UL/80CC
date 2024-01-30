// rendering.cpp
#include <Graphics/Shading/Shader.hpp>
#include <fstream>
#include <vector>
#include <iostream>

// TODO ADD TEXTURE HANDLING...
namespace ettycc
{
    Shader::Shader(const std::string &sourceCode, GLenum type) : sourceCode_(sourceCode), type_(type)
    {
        Compile();
    }

    Shader::~Shader()
    {
        if (shaderId_ != 0)
            glDeleteShader(shaderId_);
    }

    GLuint Shader::GetShaderId() const
    {
        return shaderId_;
    }

    void Shader::Compile()
    {
        shaderId_ = glCreateShader(type_);
        const char *source = sourceCode_.c_str();
        glShaderSource(shaderId_, 1, &source, nullptr);
        glCompileShader(shaderId_);

        // Check compilation status
        GLint compileStatus;
        glGetShaderiv(shaderId_, GL_COMPILE_STATUS, &compileStatus);
        if (compileStatus != GL_TRUE)
        {
            // Handle compilation error (print log, throw exception, etc.)
            GLint logLength;
            glGetShaderiv(shaderId_, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<GLchar> log(logLength);
            glGetShaderInfoLog(shaderId_, logLength, nullptr, log.data());
            std::cerr << "Shader compilation error:\n"
                      << log.data() << std::endl;
            glDeleteShader(shaderId_);
            shaderId_ = 0; // Mark shader as invalid
        }
    }
}
