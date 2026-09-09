#pragma once

#include "AssetHandle.hpp"

#include <string>
#include <GL/glew.h>
#include <GL/gl.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace ettycc
{
    // Filter mode for texture sampling
    enum class TextureFilterMode : int
    {
        Point     = 0,  // GL_NEAREST
        Bilinear  = 1,  // GL_LINEAR
        Trilinear = 2   // GL_LINEAR_MIPMAP_LINEAR
    };

    // Wrap mode for texture coordinates outside [0,1]
    enum class TextureWrapMode : int
    {
        Repeat       = 0,  // GL_REPEAT
        Clamp        = 1,  // GL_CLAMP_TO_EDGE
        MirrorRepeat = 2,  // GL_MIRRORED_REPEAT
        FitToShape   = 3   // Stretch texture to fit shape (tiling forced to 1,1; GL uses CLAMP_TO_EDGE)
    };

    // Sprite mode (future: spritesheet, animation)
    enum class SpriteMode : int
    {
        Single      = 0,
        SpriteSheet = 1
    };

    // A loaded texture asset.
    // The metadata (filter, wrap, PPU, etc.) IS the asset's persisted descriptor.
    // The .meta file on disk is just the serialized form of this struct's config fields.
    struct TextureAsset
    {
        // --- GPU state (runtime, not serialized) ---
        GLuint glHandle  = 0;
        int    width     = 0;
        int    height    = 0;
        int    channels  = 0;

        // --- Descriptor / metadata (persisted to .meta file) ---
        TextureFilterMode filterMode      = TextureFilterMode::Point;
        TextureWrapMode   wrapMode        = TextureWrapMode::FitToShape;
        SpriteMode        spriteMode      = SpriteMode::Single;
        float             pixelsPerUnit   = 100.f;
        bool              generateMipmaps = true;

        // Source dimensions (auto-populated on scan, persisted for convenience)
        int sourceWidth  = 0;
        int sourceHeight = 0;

        // ---- Meta serialization (matches the old ImageMeta format) ----

        nlohmann::json MetaToJson() const
        {
            return {
                {"filterMode",      static_cast<int>(filterMode)},
                {"wrapMode",        static_cast<int>(wrapMode)},
                {"spriteMode",      static_cast<int>(spriteMode)},
                {"pixelsPerUnit",   pixelsPerUnit},
                {"generateMipmaps", generateMipmaps},
                {"sourceWidth",     sourceWidth},
                {"sourceHeight",    sourceHeight}
            };
        }

        void MetaFromJson(const nlohmann::json& j)
        {
            if (j.contains("filterMode"))      filterMode      = static_cast<TextureFilterMode>(j["filterMode"].get<int>());
            if (j.contains("wrapMode"))         wrapMode        = static_cast<TextureWrapMode>(j["wrapMode"].get<int>());
            if (j.contains("spriteMode"))       spriteMode      = static_cast<SpriteMode>(j["spriteMode"].get<int>());
            if (j.contains("pixelsPerUnit"))    pixelsPerUnit   = j["pixelsPerUnit"].get<float>();
            if (j.contains("generateMipmaps")) generateMipmaps = j["generateMipmaps"].get<bool>();
            if (j.contains("sourceWidth"))      sourceWidth     = j["sourceWidth"].get<int>();
            if (j.contains("sourceHeight"))     sourceHeight    = j["sourceHeight"].get<int>();
        }

        bool SaveMeta(const std::string& metaPath) const
        {
            try {
                std::ofstream os(metaPath);
                if (!os.is_open()) return false;
                os << MetaToJson().dump(4);
                return true;
            } catch (...) { return false; }
        }

        bool LoadMeta(const std::string& metaPath)
        {
            try {
                std::ifstream is(metaPath);
                if (!is.is_open()) return false;
                nlohmann::json j;
                is >> j;
                MetaFromJson(j);
                return true;
            } catch (...) { return false; }
        }

        // Apply current descriptor settings to the GL texture object.
        void ApplyGLParams() const
        {
            if (glHandle == 0) return;
            glBindTexture(GL_TEXTURE_2D, glHandle);

            // Wrap (FitToShape uses CLAMP_TO_EDGE on the GL side; tiling is handled by the renderer)
            GLenum wrap = GL_REPEAT;
            switch (wrapMode)
            {
                case TextureWrapMode::Clamp:         wrap = GL_CLAMP_TO_EDGE;    break;
                case TextureWrapMode::FitToShape:    wrap = GL_CLAMP_TO_EDGE;    break;
                case TextureWrapMode::MirrorRepeat:  wrap = GL_MIRRORED_REPEAT;  break;
                default: break;
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

            // Filter
            switch (filterMode)
            {
                case TextureFilterMode::Point:
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    break;
                case TextureFilterMode::Bilinear:
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    break;
                case TextureFilterMode::Trilinear:
                default:
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    break;
            }

            glBindTexture(GL_TEXTURE_2D, 0);
        }
    };

} // namespace ettycc
