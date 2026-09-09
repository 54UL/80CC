#ifndef SPRITE_SHAPE_HPP
#define SPRITE_SHAPE_HPP

#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <queue>
#include <glm/glm.hpp>

#ifdef _MSC_VER
#include <intrin.h>
#define ETTYCC_POPCNT64(x) static_cast<int>(__popcnt64(x))
#else
#define ETTYCC_POPCNT64(x) __builtin_popcountll(x)
#endif
#include <Math/Constants.hpp>
#include <Math/Utils.hpp>
#include <Graphics/GeometryUtils.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

namespace ettycc
{
    struct SpriteShape; // forward declaration for OccupancyGrid::BuildMesh

    // =========================================================================
    // OccupancyGrid -- 2D boolean grid for volumetric rock shapes.
    //
    // Coordinate system: grid covers local space [-1, 1] x [-1, 1].
    // Each cell is (2/res) x (2/res).  Cell (r, c) maps to center:
    //   x = -1 + (c + 0.5) * 2/res
    //   y = -1 + (r + 0.5) * 2/res
    //
    // Marching squares extracts boundary as a SpriteShape with radial UVs.
    // =========================================================================
    struct OccupancyGrid
    {
        int res = 0;                     // grid resolution (res x res)

        // Bitfield storage: 1 bit per cell, packed into uint64_t words.
        // Word count = ceil(res*res / 64). Enables popcount for OccupiedCount,
        // bitwise OR for Merge, and 64-cells-at-once ops for flood fill.
        std::vector<uint64_t> words;

        OccupancyGrid() = default;
        explicit OccupancyGrid(int resolution)
            : res(resolution)
            , words((static_cast<size_t>(resolution) * resolution + 63) / 64, 0ULL) {}

        bool Get(int r, int c) const
        {
            if (r < 0 || r >= res || c < 0 || c >= res) return false;
            const size_t idx = static_cast<size_t>(r) * res + c;
            return (words[idx >> 6] >> (idx & 63)) & 1ULL;
        }
        void Set(int r, int c, bool v)
        {
            if (r < 0 || r >= res || c < 0 || c >= res) return;
            const size_t idx = static_cast<size_t>(r) * res + c;
            if (v)
                words[idx >> 6] |=   1ULL << (idx & 63);
            else
                words[idx >> 6] &= ~(1ULL << (idx & 63));
        }

        glm::vec2 CellCenter(int r, int c) const
        {
            float step = 2.f / static_cast<float>(res);
            return { -1.f + (static_cast<float>(c) + 0.5f) * step,
                     -1.f + (static_cast<float>(r) + 0.5f) * step };
        }

        // Fill a jittered circle (procedural rock).
        // roughness [0,1], seed for deterministic RNG.
        void FillRock(float roughness = 0.3f, uint32_t seed = 0)
        {
            auto rng = [state = seed == 0 ? uint32_t(0xDEADBEEF) : seed]() mutable -> float {
                state ^= state << 13; state ^= state >> 17; state ^= state << 5;
                return static_cast<float>(state & 0xFFFF) / 65535.f;
            };

            // Pre-compute jittered radii per angle
            constexpr int kAngleSamples = 64;
            float radii[kAngleSamples];
            for (int i = 0; i < kAngleSamples; ++i)
                radii[i] = 1.f - roughness * (rng() * 0.8f + 0.1f);

            const float step = 2.f / static_cast<float>(res);
            for (int r = 0; r < res; ++r)
            {
                for (int c = 0; c < res; ++c)
                {
                    float x = -1.f + (static_cast<float>(c) + 0.5f) * step;
                    float y = -1.f + (static_cast<float>(r) + 0.5f) * step;
                    float dist = glm::sqrt(x * x + y * y);
                    float angle = math::WrapAngle(glm::atan(y, x));
                    float t = angle * math::kInvTwoPi * kAngleSamples;
                    int idx = static_cast<int>(t) % kAngleSamples;
                    int idx2 = (idx + 1) % kAngleSamples;
                    float frac = t - glm::floor(t);
                    float jitteredR = radii[idx] * (1.f - frac) + radii[idx2] * frac;
                    Set(r, c, dist <= jitteredR);
                }
            }
        }

        // Smooth the shape boundary toward a geoid (near-circular with
        // organic variation). Samples radial profile, smooths, re-fills.
        // Preserves total occupied area. Good to call after multiple fusions.
        void Relax(int passes = 3)
        {
            constexpr int kSamples = 64;
            const float step = 2.f / static_cast<float>(res);
            const glm::vec2 centroid = Centroid();

            // Sample current boundary radius
            float radii[kSamples];
            for (int s = 0; s < kSamples; ++s)
            {
                float angle = math::kTwoPi * static_cast<float>(s) / kSamples;
                glm::vec2 dir = { glm::cos(angle), glm::sin(angle) };
                radii[s] = 0.f;
                for (float t = 0.f; t < 1.5f; t += step * 0.4f)
                {
                    glm::vec2 pos = centroid + dir * t;
                    int c = static_cast<int>((pos.x + 1.f) / 2.f * static_cast<float>(res));
                    int r = static_cast<int>((pos.y + 1.f) / 2.f * static_cast<float>(res));
                    if (r >= 0 && r < res && c >= 0 && c < res && Get(r, c))
                        radii[s] = t;
                }
            }

            // Smooth radii
            for (int iter = 0; iter < passes; ++iter)
            {
                float smoothed[kSamples];
                for (int s = 0; s < kSamples; ++s)
                {
                    int p1 = (s - 2 + kSamples) % kSamples;
                    int p  = (s - 1 + kSamples) % kSamples;
                    int n  = (s + 1) % kSamples;
                    int n1 = (s + 2) % kSamples;
                    smoothed[s] = (radii[p1] + 4.f * radii[p] + 6.f * radii[s]
                                 + 4.f * radii[n] + radii[n1]) / 16.f;
                }
                for (int s = 0; s < kSamples; ++s)
                    radii[s] = smoothed[s];
            }

            // Re-fill grid from smoothed profile
            for (int r = 0; r < res; ++r)
            {
                for (int c = 0; c < res; ++c)
                {
                    glm::vec2 pos = CellCenter(r, c);
                    glm::vec2 d = pos - centroid;
                    float dist = glm::length(d);

                    if (dist < 1e-6f) { Set(r, c, true); continue; }

                    float angle = math::WrapAngle(glm::atan(d.y, d.x));
                    float t = angle * math::kInvTwoPi * kSamples;
                    int idx  = static_cast<int>(t) % kSamples;
                    int idx2 = (idx + 1) % kSamples;
                    float frac = t - glm::floor(t);
                    float maxDist = radii[idx] * (1.f - frac) + radii[idx2] * frac;

                    Set(r, c, dist <= maxDist);
                }
            }
        }

        // Merge another grid into this one (OR operation).
        // offsetB: position of B's center relative to A's center in local [-1,1] space.
        // scaleRatio: size of B relative to A.
        void Merge(const OccupancyGrid& other, glm::vec2 offsetB, float scaleRatio)
        {
            // Fast path: same resolution, no offset, no scale → bitwise OR
            if (other.res == res && scaleRatio == 1.f
                && offsetB.x == 0.f && offsetB.y == 0.f)
            {
                for (size_t i = 0; i < words.size(); ++i)
                    words[i] |= other.words[i];
                return;
            }

            const float invStep = static_cast<float>(res) / 2.f;

            const float minX = offsetB.x - scaleRatio;
            const float maxX = offsetB.x + scaleRatio;
            const float minY = offsetB.y - scaleRatio;
            const float maxY = offsetB.y + scaleRatio;

            int rMin = glm::max(0, static_cast<int>((minY + 1.f) * invStep));
            int rMax = glm::min(res - 1, static_cast<int>((maxY + 1.f) * invStep));
            int cMin = glm::max(0, static_cast<int>((minX + 1.f) * invStep));
            int cMax = glm::min(res - 1, static_cast<int>((maxX + 1.f) * invStep));

            for (int r = rMin; r <= rMax; ++r)
            {
                for (int c = cMin; c <= cMax; ++c)
                {
                    if (Get(r, c)) continue;

                    glm::vec2 myPos = CellCenter(r, c);
                    glm::vec2 otherLocal = (myPos - offsetB) / scaleRatio;
                    if (otherLocal.x < -1.f || otherLocal.x > 1.f ||
                        otherLocal.y < -1.f || otherLocal.y > 1.f) continue;

                    int oc = static_cast<int>((otherLocal.x + 1.f) / 2.f * other.res);
                    int or_ = static_cast<int>((otherLocal.y + 1.f) / 2.f * other.res);
                    oc = glm::clamp(oc, 0, other.res - 1);
                    or_ = glm::clamp(or_, 0, other.res - 1);
                    if (other.Get(or_, oc))
                        Set(r, c, true);
                }
            }
        }

        // Clear cells along a line (for fracture).
        // linePoint, lineNormal in local [-1,1] space.
        // clearWidth: how many cells wide to clear.
        void ClearLine(glm::vec2 linePoint, glm::vec2 lineNormal, int clearWidth = 1)
        {
            const float step = 2.f / static_cast<float>(res);
            const float halfWidth = static_cast<float>(clearWidth) * step;
            for (int r = 0; r < res; ++r)
            {
                for (int c = 0; c < res; ++c)
                {
                    glm::vec2 pos = CellCenter(r, c);
                    float dist = glm::abs(glm::dot(pos - linePoint, lineNormal));
                    // Clear cells on the positive side of the line within clearWidth
                    float side = glm::dot(pos - linePoint, lineNormal);
                    if (side > 0.f && dist < halfWidth)
                        Set(r, c, false);
                }
            }
        }

        // Split into two grids along a line.
        // Returns (positive side, negative side).
        std::pair<OccupancyGrid, OccupancyGrid> Split(glm::vec2 linePoint, glm::vec2 lineNormal) const
        {
            OccupancyGrid sideA(res), sideB(res);
            for (int r = 0; r < res; ++r)
            {
                for (int c = 0; c < res; ++c)
                {
                    if (!Get(r, c)) continue;
                    glm::vec2 pos = CellCenter(r, c);
                    float side = glm::dot(pos - linePoint, lineNormal);
                    if (side >= 0.f)
                        sideA.Set(r, c, true);
                    else
                        sideB.Set(r, c, true);
                }
            }
            return { sideA, sideB };
        }

        // Flood-fill to find connected components (for shatter).
        // Uses bitfield for visited set (same layout as grid words).
        std::vector<OccupancyGrid> ConnectedComponents() const
        {
            std::vector<OccupancyGrid> result;
            const size_t totalCells = static_cast<size_t>(res) * res;
            const size_t numWords = (totalCells + 63) / 64;
            std::vector<uint64_t> visited(numWords, 0ULL);

            auto isVisited = [&](size_t idx) -> bool {
                return (visited[idx >> 6] >> (idx & 63)) & 1ULL;
            };
            auto markVisited = [&](size_t idx) {
                visited[idx >> 6] |= 1ULL << (idx & 63);
            };

            for (int r = 0; r < res; ++r)
            {
                for (int c = 0; c < res; ++c)
                {
                    const size_t cellIdx = static_cast<size_t>(r) * res + c;
                    if (!Get(r, c) || isVisited(cellIdx)) continue;

                    OccupancyGrid comp(res);
                    std::queue<std::pair<int, int>> q;
                    q.push({ r, c });
                    markVisited(cellIdx);

                    while (!q.empty())
                    {
                        auto [cr, cc] = q.front(); q.pop();
                        comp.Set(cr, cc, true);

                        static const int dr[] = { -1, 1, 0, 0 };
                        static const int dc[] = { 0, 0, -1, 1 };
                        for (int d = 0; d < 4; ++d)
                        {
                            int nr = cr + dr[d], nc = cc + dc[d];
                            if (nr >= 0 && nr < res && nc >= 0 && nc < res)
                            {
                                const size_t nIdx = static_cast<size_t>(nr) * res + nc;
                                if (Get(nr, nc) && !isVisited(nIdx))
                                {
                                    markVisited(nIdx);
                                    q.push({ nr, nc });
                                }
                            }
                        }
                    }

                    result.push_back(std::move(comp));
                }
            }
            return result;
        }

        // Count occupied cells -- hardware popcount over bitfield (~50x faster)
        int OccupiedCount() const
        {
            int count = 0;
            for (uint64_t w : words)
                count += ETTYCC_POPCNT64(w);
            return count;
        }

        // Compute centroid of occupied cells in local [-1,1] space
        glm::vec2 Centroid() const
        {
            glm::vec2 sum(0.f);
            int count = 0;
            for (int r = 0; r < res; ++r)
                for (int c = 0; c < res; ++c)
                    if (Get(r, c)) { sum += CellCenter(r, c); ++count; }
            return count > 0 ? sum / static_cast<float>(count) : glm::vec2(0.f);
        }

        // Grow shape by adding area, biased toward `toward` direction.
        // Works on the radial profile (like a geoid): samples boundary radius
        // at uniform angles, increases radii (valleys get filled first via
        // smoothing), then re-fills the grid from the smooth profile.
        // This keeps the shape organic regardless of grid resolution.
        void GrowToward(glm::vec2 toward, int cellsToAdd)
        {
            if (cellsToAdd <= 0) return;

            constexpr int kSamples = 64;
            const float step = 2.f / static_cast<float>(res);
            const glm::vec2 centroid = Centroid();

            // --- 1. Sample current boundary radius at uniform angles ---
            float radii[kSamples];
            for (int s = 0; s < kSamples; ++s)
            {
                float angle = math::kTwoPi * static_cast<float>(s) / kSamples;
                glm::vec2 dir = { glm::cos(angle), glm::sin(angle) };

                // Ray-march outward from centroid, find last occupied cell
                radii[s] = 0.f;
                for (float t = 0.f; t < 1.5f; t += step * 0.4f)
                {
                    glm::vec2 pos = centroid + dir * t;
                    int c = static_cast<int>((pos.x + 1.f) / 2.f * static_cast<float>(res));
                    int r = static_cast<int>((pos.y + 1.f) / 2.f * static_cast<float>(res));
                    if (r >= 0 && r < res && c >= 0 && c < res && Get(r, c))
                        radii[s] = t;
                }
            }

            // --- 2. Compute how much to grow each radius ---
            // The total area added should roughly match cellsToAdd cells.
            // Area of each cell = step^2, so target area = cellsToAdd * step^2.
            // We distribute growth with a cosine bias toward the impact angle,
            // then smooth so valleys fill before peaks grow.
            float impactAngle = math::WrapAngle(glm::atan(toward.y - centroid.y,
                                                           toward.x - centroid.x));
            float targetArea = static_cast<float>(cellsToAdd) * step * step;

            // Compute current total area from radial profile (sum of pie slices)
            float sliceAngle = math::kTwoPi / static_cast<float>(kSamples);
            float currentArea = 0.f;
            for (int s = 0; s < kSamples; ++s)
                currentArea += 0.5f * radii[s] * radii[s] * sliceAngle;

            // Target total area
            float newTotalArea = currentArea + targetArea;

            // Uniform growth factor to reach target area (isotropic baseline)
            float areaRatio = (currentArea > 1e-6f) ? newTotalArea / currentArea : 1.f;
            float uniformGrowth = glm::sqrt(areaRatio); // radius scales as sqrt(area)

            // Apply growth with directional bias
            float growth[kSamples];
            for (int s = 0; s < kSamples; ++s)
            {
                float angle = math::kTwoPi * static_cast<float>(s) / kSamples;
                // Cosine bias: strongest growth toward impact, weakest opposite
                float angleDiff = math::WrapAngle(angle - impactAngle);
                if (angleDiff > math::kPi) angleDiff -= math::kTwoPi;
                float bias = 0.5f + 0.5f * glm::cos(angleDiff); // [0,1]

                // Mix: 40% uniform growth + 60% directional bias
                float factor = 0.4f * uniformGrowth + 0.6f * (1.f + (uniformGrowth - 1.f) * bias * 2.f);
                growth[s] = radii[s] * factor;
            }

            // --- 3. Relaxation: smooth radii so valleys fill before peaks grow ---
            // Multiple passes of weighted average (simulates gravity settling)
            for (int iter = 0; iter < 4; ++iter)
            {
                float smoothed[kSamples];
                for (int s = 0; s < kSamples; ++s)
                {
                    int p1 = (s - 2 + kSamples) % kSamples;
                    int p  = (s - 1 + kSamples) % kSamples;
                    int n  = (s + 1) % kSamples;
                    int n1 = (s + 2) % kSamples;
                    // Gaussian-like kernel [1,4,6,4,1]/16
                    smoothed[s] = (growth[p1] + 4.f * growth[p] + 6.f * growth[s]
                                 + 4.f * growth[n] + growth[n1]) / 16.f;
                }
                for (int s = 0; s < kSamples; ++s)
                    growth[s] = smoothed[s];
            }

            // Clamp to grid bounds
            for (int s = 0; s < kSamples; ++s)
                growth[s] = glm::min(growth[s], 0.98f);

            // --- 4. Re-fill grid from the smoothed radial profile ---
            for (int r = 0; r < res; ++r)
            {
                for (int c = 0; c < res; ++c)
                {
                    glm::vec2 pos = CellCenter(r, c);
                    glm::vec2 d = pos - centroid;
                    float dist = glm::length(d);

                    if (dist < 1e-6f) { Set(r, c, true); continue; }

                    float angle = math::WrapAngle(glm::atan(d.y, d.x));
                    float t = angle * math::kInvTwoPi * kSamples;
                    int idx  = static_cast<int>(t) % kSamples;
                    int idx2 = (idx + 1) % kSamples;
                    float frac = t - glm::floor(t);
                    float maxDist = growth[idx] * (1.f - frac) + growth[idx2] * frac;

                    // Only add cells, never remove existing ones (accretion)
                    if (dist <= maxDist)
                        Set(r, c, true);
                }
            }
        }

        // ---- Marching Squares mesh extraction --------------------------------
        // Extracts a SpriteShape from the grid boundary using marching squares.
        // Produces smooth boundary with radial UVs for procedural rendering.
        SpriteShape BuildMesh() const;

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(res));
            // Serialize bitfield as uint8_t proxy for JSON readability
            const size_t totalCells = static_cast<size_t>(res) * res;
            std::vector<uint8_t> proxy(totalCells);
            // Save: unpack bits → bytes
            for (size_t i = 0; i < totalCells; ++i)
                proxy[i] = (words.size() > (i >> 6))
                         ? static_cast<uint8_t>((words[i >> 6] >> (i & 63)) & 1ULL) : 0;
            ar(cereal::make_nvp("cells", proxy));
            // Load: pack bytes → bits
            if (proxy.size() == totalCells)
            {
                words.assign((totalCells + 63) / 64, 0ULL);
                for (size_t i = 0; i < totalCells; ++i)
                    if (proxy[i])
                        words[i >> 6] |= 1ULL << (i & 63);
            }
        }
    };
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

        // Volumetric grid -- only populated for Custom/Rock shapes.
        // Used for fusion (OR), fracture (split), shatter (flood-fill).
        OccupancyGrid grid_;

        // -- Factory methods ----------------------------------------------

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

            const float step = math::kTwoPi / static_cast<float>(segments);
            for (int i = 0; i < segments; ++i)
            {
                float angle = step * static_cast<float>(i);
                float x = glm::cos(angle);
                float y = glm::sin(angle);
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

        // Procedural rocky shape using occupancy grid + marching squares.
        // Defined out-of-line after struct is complete (needs full SpriteShape type).
        static SpriteShape MakeRock(int segments = 16, float roughness = 0.3f,
                                     uint32_t seed = 0, int gridRes = 16);

        // Compute the 2D area
        float ComputeArea() const
        {
            float area = 0.f;
            for (size_t t = 0; t < indices.size() / 3; ++t)
            {
                const glm::vec2& a = vertices[indices[t * 3 + 0]].position;
                const glm::vec2& b = vertices[indices[t * 3 + 1]].position;
                const glm::vec2& c = vertices[indices[t * 3 + 2]].position;
                area += glm::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y));
            }
            return area * 0.5f;
        }

        // Compute the 2D centroid of the shape (area-weighted average)
        glm::vec2 ComputeCentroid() const
        {
            glm::vec2 centroid(0.f);
            float totalArea = 0.f;
            for (size_t t = 0; t < indices.size() / 3; ++t)
            {
                const glm::vec2& a = vertices[indices[t * 3 + 0]].position;
                const glm::vec2& b = vertices[indices[t * 3 + 1]].position;
                const glm::vec2& c = vertices[indices[t * 3 + 2]].position;
                float triArea = glm::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
                centroid += triArea * (a + b + c) / 3.f;
                totalArea += triArea;
            }
            return (totalArea > 0.f) ? centroid / totalArea : glm::vec2(0.f);
        }

        // Recompute UVs as radial encoding: u = normalized dist, v = angle/2pi.
        // Assumes fan layout with centroid at vertices[0].
        // All boundary vertices (indices 1..N) get u = 1.0 so the shader
        // outline appears consistently at the shape edge regardless of shape.
        void RecomputeRadialUVs()
        {
            if (vertices.empty()) return;
            glm::vec2 centroid = vertices[0].position;

            vertices[0].uv = { 0.f, 0.f };
            for (size_t i = 1; i < vertices.size(); ++i)
            {
                glm::vec2 d = vertices[i].position - centroid;
                float angle = math::AngleToNormalized(glm::atan(d.y, d.x));
                // All boundary vertices are at r=1.0 -- the GPU interpolates
                // linearly from 0 (centroid) to 1 (boundary) across each triangle.
                vertices[i].uv = { 1.f, angle };
            }
        }

        // Bounding radius from centroid (first vertex in fan layout)
        float BoundingRadius() const
        {
            if (vertices.empty()) return 0.f;
            glm::vec2 c = vertices[0].position;
            float maxR = 0.f;
            for (size_t i = 1; i < vertices.size(); ++i)
                maxR = glm::max(maxR, glm::length(vertices[i].position - c));
            return maxR;
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
               CEREAL_NVP(circleSegments),
               CEREAL_NVP(grid_));
        }
    };

    // -- Out-of-line definition (needs complete SpriteShape type) ---------------
    inline SpriteShape SpriteShape::MakeRock(int segments, float roughness,
                                              uint32_t seed, int gridRes)
    {
        (void)segments; // legacy parameter, replaced by gridRes

        OccupancyGrid grid(gridRes);
        grid.FillRock(roughness, seed);

        SpriteShape s = grid.BuildMesh();
        s.preset = Preset::Custom;
        s.name   = "Rock";
        s.grid_  = std::move(grid);
        return s;
    }

    // -- Merge two shapes using occupancy grids (OR operation).
    // offsetB: position of B relative to A in A's local space [-1,1].
    // scaleRatio: scale of B relative to A (e.g. B.halfExt / A.halfExt).
    // If both shapes have grids, uses grid OR → marching squares.
    // Falls back to convex hull if grids are missing.
    inline SpriteShape MergeSpriteShapes(const SpriteShape& a, const SpriteShape& b,
                                          glm::vec2 offsetB, float scaleRatio)
    {
        // Grid-based merge (preferred path)
        if (a.grid_.res > 0 && b.grid_.res > 0)
        {
            OccupancyGrid merged = a.grid_; // copy A's grid
            merged.Merge(b.grid_, offsetB, scaleRatio);

            SpriteShape result = merged.BuildMesh();
            result.preset = SpriteShape::Preset::Custom;
            result.name   = "Merged";
            result.grid_  = std::move(merged);
            return result;
        }

        // Fallback: convex hull (for non-grid shapes)
        std::vector<glm::vec2> points;
        points.reserve(a.vertices.size() + b.vertices.size());
        for (auto& v : a.vertices)
            points.push_back(v.position);
        for (auto& v : b.vertices)
            points.push_back(v.position * scaleRatio + offsetB);

        auto hull = geo::ConvexHull(points);
        if (hull.size() < 3) return a;
        auto fan = geo::FanTriangulate(hull);

        SpriteShape result;
        result.preset = SpriteShape::Preset::Custom;
        result.name   = "Merged";
        for (auto& p : fan.vertices)
            result.vertices.push_back({ p, { 0.f, 0.f } });
        result.indices = fan.indices;
        result.RecomputeRadialUVs();
        return result;
    }

    // -- Slice: cut shape along a 2D line ---------------------------------
    // linePoint:  a point on the cutting line (local space [-1,1])
    // lineNormal: unit normal of the line (points toward side A)
    // Returns two shapes. If the line misses the mesh, one side is empty.
    struct SpriteShapeSliceResult {
        SpriteShape sideA; // positive side (normal direction)
        SpriteShape sideB; // negative side
    };

    inline SpriteShapeSliceResult SliceSpriteShape(const SpriteShape& src, glm::vec2 linePoint, glm::vec2 lineNormal)
    {
        SpriteShapeSliceResult result;

        // Grid-based slice (preferred path)
        if (src.grid_.res > 0)
        {
            auto [gridA, gridB] = src.grid_.Split(linePoint, lineNormal);

            if (gridA.OccupiedCount() >= 3)
            {
                result.sideA = gridA.BuildMesh();
                result.sideA.preset = SpriteShape::Preset::Custom;
                result.sideA.name   = "Sliced";
                result.sideA.grid_  = std::move(gridA);
            }

            if (gridB.OccupiedCount() >= 3)
            {
                result.sideB = gridB.BuildMesh();
                result.sideB.preset = SpriteShape::Preset::Custom;
                result.sideB.name   = "Sliced";
                result.sideB.grid_  = std::move(gridB);
            }

            return result;
        }

        // Fallback: triangle-clipping slice (for non-grid shapes)
        result.sideA.preset = SpriteShape::Preset::Custom;
        result.sideA.name   = "Sliced";
        result.sideB.preset = SpriteShape::Preset::Custom;
        result.sideB.name   = "Sliced";

        auto lerpVert = [](const SpriteVertex& a, const SpriteVertex& b, float t) -> SpriteVertex {
            return { a.position + t * (b.position - a.position),
                     a.uv       + t * (b.uv       - a.uv) };
        };

        auto signedDist = [&](const glm::vec2& p) -> float {
            return glm::dot(p - linePoint, lineNormal);
        };

        auto addTri = [](SpriteShape& shape, const SpriteVertex& v0,
                         const SpriteVertex& v1, const SpriteVertex& v2)
        {
            auto addVert = [&](const SpriteVertex& v) -> unsigned int {
                for (int k = static_cast<int>(shape.vertices.size()) - 1;
                     k >= 0 && k >= static_cast<int>(shape.vertices.size()) - 6; --k)
                {
                    const auto& ev = shape.vertices[k];
                    if (glm::abs(ev.position.x - v.position.x) < 1e-5f &&
                        glm::abs(ev.position.y - v.position.y) < 1e-5f)
                        return static_cast<unsigned int>(k);
                }
                shape.vertices.push_back(v);
                return static_cast<unsigned int>(shape.vertices.size() - 1);
            };
            shape.indices.push_back(addVert(v0));
            shape.indices.push_back(addVert(v1));
            shape.indices.push_back(addVert(v2));
        };

        const size_t triCount = src.indices.size() / 3;
        for (size_t t = 0; t < triCount; ++t)
        {
            const SpriteVertex& v0 = src.vertices[src.indices[t * 3 + 0]];
            const SpriteVertex& v1 = src.vertices[src.indices[t * 3 + 1]];
            const SpriteVertex& v2 = src.vertices[src.indices[t * 3 + 2]];

            float d0 = signedDist(v0.position);
            float d1 = signedDist(v1.position);
            float d2 = signedDist(v2.position);

            bool a0 = d0 >= 0.f, a1 = d1 >= 0.f, a2 = d2 >= 0.f;
            int sideACount = (int)a0 + (int)a1 + (int)a2;

            if (sideACount == 3) { addTri(result.sideA, v0, v1, v2); }
            else if (sideACount == 0) { addTri(result.sideB, v0, v1, v2); }
            else {
                const SpriteVertex* tri[3] = { &v0, &v1, &v2 };
                float dists[3] = { d0, d1, d2 };
                bool sides[3] = { a0, a1, a2 };

                int solo = 0;
                if (sideACount == 1) { for (int i = 0; i < 3; ++i) if (sides[i]) { solo = i; break; } }
                else { for (int i = 0; i < 3; ++i) if (!sides[i]) { solo = i; break; } }

                int i1 = (solo + 1) % 3;
                int i2 = (solo + 2) % 3;
                float t1 = dists[solo] / (dists[solo] - dists[i1]);
                float t2 = dists[solo] / (dists[solo] - dists[i2]);
                SpriteVertex m1 = lerpVert(*tri[solo], *tri[i1], t1);
                SpriteVertex m2 = lerpVert(*tri[solo], *tri[i2], t2);

                SpriteShape& soloSide = (sideACount == 1) ? result.sideA : result.sideB;
                addTri(soloSide, *tri[solo], m1, m2);
                SpriteShape& otherSide = (sideACount == 1) ? result.sideB : result.sideA;
                addTri(otherSide, m1, *tri[i1], *tri[i2]);
                addTri(otherSide, m1, *tri[i2], m2);
            }
        }

        return result;
    }

} // namespace ettycc

#endif
