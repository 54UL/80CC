#ifndef SPRITE_SHAPE_HPP
#define SPRITE_SHAPE_HPP

#include <vector>
#include <string>
#include <cmath>
#include <glm/glm.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

namespace ettycc
{
    // A single vertex in the sprite mesh: 2D position + UV coordinate.
    struct SpriteVertex
    {
        glm::vec2 position { 0.f, 0.f };
        glm::vec2 uv       { 0.f, 0.f };

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(cereal::make_nvp("px", position.x),
               cereal::make_nvp("py", position.y),
               cereal::make_nvp("u",  uv.x),
               cereal::make_nvp("v",  uv.y));
        }
    };

    // Describes the shape of a sprite as a triangle mesh.
    // Positions are in local space [-1, 1].
    struct SpriteShape
    {
        enum class Preset { Quad, Triangle, Circle, Custom };

        Preset                      preset   = Preset::Quad;
        std::string                 name     = "Quad";
        std::vector<SpriteVertex>   vertices;
        std::vector<unsigned int>   indices;
        int                         circleSegments = 32; // only used for circle preset

        // ── Factory methods ──────────────────────────────────────────────

        static SpriteShape MakeQuad()
        {
            SpriteShape s;
            s.preset = Preset::Quad;
            s.name   = "Quad";
            s.vertices = {
                { { -1.f,  1.f }, { 0.f, 1.f } },  // top-left
                { {  1.f,  1.f }, { 1.f, 1.f } },  // top-right
                { {  1.f, -1.f }, { 1.f, 0.f } },  // bottom-right
                { { -1.f, -1.f }, { 0.f, 0.f } },  // bottom-left
            };
            s.indices = { 0, 1, 2, 2, 3, 0 };
            return s;
        }

        static SpriteShape MakeTriangle()
        {
            SpriteShape s;
            s.preset = Preset::Triangle;
            s.name   = "Triangle";
            s.vertices = {
                { {  0.f,  1.f }, { 0.5f, 1.f } },  // top
                { {  1.f, -1.f }, { 1.0f, 0.f } },  // bottom-right
                { { -1.f, -1.f }, { 0.0f, 0.f } },  // bottom-left
            };
            s.indices = { 0, 1, 2 };
            return s;
        }

        static SpriteShape MakeCircle(int segments = 32)
        {
            SpriteShape s;
            s.preset        = Preset::Circle;
            s.name          = "Circle";
            s.circleSegments = segments;

            // Center vertex
            s.vertices.push_back({ { 0.f, 0.f }, { 0.5f, 0.5f } });

            const float step = 2.f * 3.14159265f / static_cast<float>(segments);
            for (int i = 0; i < segments; ++i)
            {
                float angle = step * static_cast<float>(i);
                float x = std::cos(angle);
                float y = std::sin(angle);
                s.vertices.push_back({
                    { x, y },
                    { x * 0.5f + 0.5f, y * 0.5f + 0.5f }
                });
            }

            for (int i = 1; i <= segments; ++i)
            {
                s.indices.push_back(0);
                s.indices.push_back(static_cast<unsigned int>(i));
                s.indices.push_back(static_cast<unsigned int>(i % segments + 1));
            }

            return s;
        }

        // Build the interleaved float buffer (x, y, z, u, v) for GL upload.
        std::vector<float> BuildVertexBuffer() const
        {
            std::vector<float> buf;
            buf.reserve(vertices.size() * 5);
            for (auto& v : vertices)
            {
                buf.push_back(v.position.x);
                buf.push_back(v.position.y);
                buf.push_back(0.f); // z = 0 for 2D sprites
                buf.push_back(v.uv.x);
                buf.push_back(v.uv.y);
            }
            return buf;
        }

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(preset),
               CEREAL_NVP(name),
               CEREAL_NVP(vertices),
               CEREAL_NVP(indices),
               CEREAL_NVP(circleSegments));
        }
    };

} // namespace ettycc

#endif
