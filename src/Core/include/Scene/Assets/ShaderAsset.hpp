#pragma once

#include "AssetHandle.hpp"
#include <Graphics/Shading/ShaderPipeline.hpp>

#include <GL/glew.h>
#include <GL/gl.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

namespace ettycc
{
    // Introspection data for a single active uniform or attribute.
    struct ShaderUniformInfo
    {
        std::string name;
        GLenum      type     = 0;   // GL_FLOAT, GL_FLOAT_VEC3, GL_SAMPLER_2D, etc.
        GLint       location = -1;
        GLint       size     = 1;   // array size (1 for non-arrays)
    };

    struct ShaderAttribInfo
    {
        std::string name;
        GLenum      type     = 0;
        GLint       location = -1;
    };

    // A compiled shader program asset.
    // Shared by all renderables that use the same vertex/fragment pair.
    struct ShaderAsset
    {
        ShaderPipeline pipeline;
        GLuint         programId = 0;

        // Source code (kept for potential hot-reload)
        std::string vertSource;
        std::string fragSource;

        // Introspection: populated after linking
        std::vector<ShaderUniformInfo> uniforms;
        std::vector<ShaderAttribInfo>  attribs;

        // Uniforms that are engine-managed (not exposed to materials/inspector).
        // Populated by Introspect().
        std::vector<ShaderUniformInfo> userUniforms;

        int Bind()   { return pipeline.Bind(); }
        int Unbind() { return pipeline.Unbind(); }

        // Enumerate all active uniforms and attributes from the linked program.
        // Call after Create()/linking. Fills uniforms, attribs, and userUniforms.
        void Introspect()
        {
            uniforms.clear();
            attribs.clear();
            userUniforms.clear();

            if (programId == 0) return;

            // Engine-managed uniform names -- not exposed to material editor
            static const std::unordered_set<std::string> kEngineUniforms = {
                "PVM", "uPV", "time", "deltaTime",
                "ourTexture", "tiling", "uPickerId"
            };

            // -- Uniforms --
            GLint uniformCount = 0;
            glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &uniformCount);

            for (GLint i = 0; i < uniformCount; ++i)
            {
                char nameBuf[256];
                GLsizei nameLen = 0;
                GLint   arrSize = 0;
                GLenum  uType  = 0;

                glGetActiveUniform(programId, static_cast<GLuint>(i),
                                   sizeof(nameBuf), &nameLen, &arrSize, &uType, nameBuf);

                std::string uName(nameBuf, nameLen);
                // Strip "[0]" suffix from array uniforms
                auto bracket = uName.find('[');
                if (bracket != std::string::npos)
                    uName = uName.substr(0, bracket);

                GLint loc = glGetUniformLocation(programId, uName.c_str());

                ShaderUniformInfo info{ uName, uType, loc, arrSize };
                uniforms.push_back(info);

                if (kEngineUniforms.find(uName) == kEngineUniforms.end())
                    userUniforms.push_back(info);
            }

            // -- Attributes --
            GLint attribCount = 0;
            glGetProgramiv(programId, GL_ACTIVE_ATTRIBUTES, &attribCount);

            for (GLint i = 0; i < attribCount; ++i)
            {
                char nameBuf[256];
                GLsizei nameLen = 0;
                GLint   arrSize = 0;
                GLenum  aType  = 0;

                glGetActiveAttrib(programId, static_cast<GLuint>(i),
                                  sizeof(nameBuf), &nameLen, &arrSize, &aType, nameBuf);

                std::string aName(nameBuf, nameLen);
                GLint loc = glGetAttribLocation(programId, aName.c_str());

                attribs.push_back({ aName, aType, loc });
            }
        }

        // Get a human-readable string for a GL type enum.
        static const char* GLTypeName(GLenum type)
        {
            switch (type)
            {
                case GL_FLOAT:        return "float";
                case GL_FLOAT_VEC2:   return "vec2";
                case GL_FLOAT_VEC3:   return "vec3";
                case GL_FLOAT_VEC4:   return "vec4";
                case GL_INT:          return "int";
                case GL_BOOL:         return "bool";
                case GL_FLOAT_MAT3:   return "mat3";
                case GL_FLOAT_MAT4:   return "mat4";
                case GL_SAMPLER_2D:   return "sampler2D";
                default:              return "unknown";
            }
        }
    };

} // namespace ettycc
