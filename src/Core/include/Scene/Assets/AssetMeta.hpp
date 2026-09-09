#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace ettycc
{
    // Filter mode for texture sampling
    enum class TextureFilterMode : int
    {
        Point    = 0,  // GL_NEAREST -- pixel-perfect, no blurring (pixel art)
        Bilinear = 1,  // GL_LINEAR  -- smooth interpolation
        Trilinear = 2  // GL_LINEAR_MIPMAP_LINEAR -- smooth with mipmap blending
    };

    // Wrap mode for texture coordinates outside [0,1]
    enum class TextureWrapMode : int
    {
        Repeat      = 0,  // GL_REPEAT
        Clamp       = 1,  // GL_CLAMP_TO_EDGE
        MirrorRepeat = 2  // GL_MIRRORED_REPEAT
    };

    // Sprite mode (future: spritesheet, animation)
    enum class SpriteMode : int
    {
        Single      = 0,
        SpriteSheet = 1  // future: atlas with sub-rects
    };

    // Metadata for an image asset. Stored as <filename>.meta alongside the image.
    // Controls how the image is loaded to GPU and rendered as a sprite.
    struct ImageMeta
    {
        TextureFilterMode filterMode  = TextureFilterMode::Bilinear;
        TextureWrapMode   wrapMode    = TextureWrapMode::Repeat;
        SpriteMode        spriteMode  = SpriteMode::Single;
        float             pixelsPerUnit = 100.f;  // how many image pixels fit in 1 world unit
        bool              generateMipmaps = true;

        // Source image dimensions (populated on scan, read-only)
        int sourceWidth  = 0;
        int sourceHeight = 0;

        nlohmann::json ToJson() const
        {
            return {
                {"filterMode",     static_cast<int>(filterMode)},
                {"wrapMode",       static_cast<int>(wrapMode)},
                {"spriteMode",     static_cast<int>(spriteMode)},
                {"pixelsPerUnit",  pixelsPerUnit},
                {"generateMipmaps", generateMipmaps},
                {"sourceWidth",    sourceWidth},
                {"sourceHeight",   sourceHeight}
            };
        }

        static ImageMeta FromJson(const nlohmann::json& j)
        {
            ImageMeta m;
            if (j.contains("filterMode"))      m.filterMode      = static_cast<TextureFilterMode>(j["filterMode"].get<int>());
            if (j.contains("wrapMode"))         m.wrapMode        = static_cast<TextureWrapMode>(j["wrapMode"].get<int>());
            if (j.contains("spriteMode"))       m.spriteMode      = static_cast<SpriteMode>(j["spriteMode"].get<int>());
            if (j.contains("pixelsPerUnit"))    m.pixelsPerUnit   = j["pixelsPerUnit"].get<float>();
            if (j.contains("generateMipmaps")) m.generateMipmaps = j["generateMipmaps"].get<bool>();
            if (j.contains("sourceWidth"))      m.sourceWidth     = j["sourceWidth"].get<int>();
            if (j.contains("sourceHeight"))     m.sourceHeight    = j["sourceHeight"].get<int>();
            return m;
        }

        bool Save(const std::string& metaPath) const
        {
            try
            {
                std::ofstream os(metaPath);
                if (!os.is_open()) return false;
                os << ToJson().dump(4);
                return true;
            }
            catch (...) { return false; }
        }

        static bool Load(const std::string& metaPath, ImageMeta& out)
        {
            try
            {
                std::ifstream is(metaPath);
                if (!is.is_open()) return false;
                nlohmann::json j;
                is >> j;
                out = FromJson(j);
                return true;
            }
            catch (...) { return false; }
        }
    };

} // namespace ettycc
