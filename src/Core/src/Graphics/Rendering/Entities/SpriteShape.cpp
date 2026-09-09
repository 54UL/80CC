#include <Graphics/Rendering/Entities/SpriteShape.hpp>
#include <Graphics/GeometryUtils.hpp>
#include <Math/Utils.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ettycc
{

// ============================================================================
// Marching Squares mesh extraction from OccupancyGrid
//
// Produces a SpriteShape with:
//   - Fan-triangulated mesh (centroid at vertices[0])
//   - Radial UVs for procedural rendering
//   - Boundary vertices ordered counter-clockwise
//
// The algorithm:
//   1. Walk the grid border to extract contour segments
//   2. Chain segments into an ordered boundary loop
//   3. Simplify the boundary (Douglas-Peucker)
//   4. Fan-triangulate from centroid with radial UVs
// ============================================================================

SpriteShape OccupancyGrid::BuildMesh() const
{
    SpriteShape result;
    result.preset = SpriteShape::Preset::Custom;
    result.name   = "GridMesh";

    if (res < 2)
    {
        result = SpriteShape::MakeCircle(16);
        result.preset = SpriteShape::Preset::Custom;
        return result;
    }

    const float step = 2.f / static_cast<float>(res);

    // ---- Step 1: Extract boundary edges using marching squares ----
    // For each 2x2 cell group, classify the 4 corners and emit edge segments.
    // Corner layout:  TL(r+1,c) -- TR(r+1,c+1)
    //                  |              |
    //                 BL(r,c)   -- BR(r,c+1)
    // Each corner maps to its cell's occupancy (or false if outside grid).

    struct Edge {
        glm::vec2 a, b;
    };
    std::vector<Edge> edges;

    auto cornerPos = [&](int r, int c) -> glm::vec2 {
        return { -1.f + static_cast<float>(c) * step,
                 -1.f + static_cast<float>(r) * step };
    };

    auto sample = [&](int r, int c) -> bool {
        if (r < 0 || r >= res || c < 0 || c >= res) return false;
        return Get(r, c);
    };

    // Walk grid + 1 border cells to capture boundary at edges
    for (int r = -1; r < res; ++r)
    {
        for (int c = -1; c < res; ++c)
        {
            // 4 corners of this marching square cell
            bool bl = sample(r, c);
            bool br = sample(r, c + 1);
            bool tl = sample(r + 1, c);
            bool tr = sample(r + 1, c + 1);

            int caseIndex = (bl ? 1 : 0) | (br ? 2 : 0) | (tr ? 4 : 0) | (tl ? 8 : 0);
            if (caseIndex == 0 || caseIndex == 15) continue;

            glm::vec2 pBL = cornerPos(r, c);
            glm::vec2 pBR = cornerPos(r, c + 1);
            glm::vec2 pTR = cornerPos(r + 1, c + 1);
            glm::vec2 pTL = cornerPos(r + 1, c);

            // Edge midpoints
            glm::vec2 bottom = (pBL + pBR) * 0.5f;
            glm::vec2 right  = (pBR + pTR) * 0.5f;
            glm::vec2 top    = (pTL + pTR) * 0.5f;
            glm::vec2 left   = (pBL + pTL) * 0.5f;

            // Standard marching squares edge table
            switch (caseIndex)
            {
            case 1:  edges.push_back({left, bottom}); break;
            case 2:  edges.push_back({bottom, right}); break;
            case 3:  edges.push_back({left, right}); break;
            case 4:  edges.push_back({right, top}); break;
            case 5:  // saddle
                edges.push_back({left, top});
                edges.push_back({bottom, right});
                break;
            case 6:  edges.push_back({bottom, top}); break;
            case 7:  edges.push_back({left, top}); break;
            case 8:  edges.push_back({top, left}); break;
            case 9:  edges.push_back({top, bottom}); break;
            case 10: // saddle
                edges.push_back({top, right});
                edges.push_back({left, bottom});
                break;
            case 11: edges.push_back({top, right}); break;
            case 12: edges.push_back({right, left}); break;
            case 13: edges.push_back({right, bottom}); break;
            case 14: edges.push_back({bottom, left}); break;
            }
        }
    }

    if (edges.empty())
    {
        result = SpriteShape::MakeCircle(8);
        result.preset = SpriteShape::Preset::Custom;
        return result;
    }

    // ---- Step 2: Chain edges into an ordered boundary loop ----
    // Use a spatial hash to connect edge endpoints

    struct Vec2Hash {
        size_t operator()(const glm::vec2& v) const {
            // Quantize to avoid floating point issues
            int ix = static_cast<int>(glm::round(v.x * 10000.f));
            int iy = static_cast<int>(glm::round(v.y * 10000.f));
            size_t h = std::hash<int>()(ix);
            h ^= std::hash<int>()(iy) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    struct Vec2Eq {
        bool operator()(const glm::vec2& a, const glm::vec2& b) const {
            return glm::abs(a.x - b.x) < 1e-4f && glm::abs(a.y - b.y) < 1e-4f;
        }
    };

    // Build adjacency: endpoint -> list of (other endpoint, edge index)
    std::unordered_multimap<glm::vec2, std::pair<glm::vec2, size_t>, Vec2Hash, Vec2Eq> adj;
    for (size_t i = 0; i < edges.size(); ++i)
    {
        adj.insert({ edges[i].a, { edges[i].b, i } });
        adj.insert({ edges[i].b, { edges[i].a, i } });
    }

    std::vector<bool> used(edges.size(), false);
    std::vector<glm::vec2> boundary;

    // Find longest chain
    for (size_t startEdge = 0; startEdge < edges.size(); ++startEdge)
    {
        if (used[startEdge]) continue;

        std::vector<glm::vec2> chain;
        chain.push_back(edges[startEdge].a);
        chain.push_back(edges[startEdge].b);
        used[startEdge] = true;

        // Walk forward
        bool extended = true;
        while (extended)
        {
            extended = false;
            glm::vec2 tip = chain.back();
            auto range = adj.equal_range(tip);
            for (auto it = range.first; it != range.second; ++it)
            {
                if (!used[it->second.second])
                {
                    used[it->second.second] = true;
                    chain.push_back(it->second.first);
                    extended = true;
                    break;
                }
            }
        }

        if (chain.size() > boundary.size())
            boundary = chain;
    }

    if (boundary.size() < 3)
    {
        result = SpriteShape::MakeCircle(8);
        result.preset = SpriteShape::Preset::Custom;
        return result;
    }

    // ---- Step 3: Simplify boundary (Douglas-Peucker) ----
    // Reduce vertex count while preserving shape. Target ~24-32 verts.
    float epsilon = step * 0.3f; // start with small tolerance
    std::vector<glm::vec2> simplified = boundary;

    // Iterative simplification until we're under target vertex count
    constexpr int kMaxBoundaryVerts = 20;
    for (int iter = 0; iter < 10 && static_cast<int>(simplified.size()) > kMaxBoundaryVerts; ++iter)
    {
        std::vector<glm::vec2> next;
        next.push_back(simplified.front());

        for (size_t i = 1; i < simplified.size() - 1; ++i)
        {
            // Skip point if it's close to the line between prev and next
            glm::vec2 prev = next.back();
            glm::vec2 curr = simplified[i];
            glm::vec2 succ = simplified[glm::min(i + 1, simplified.size() - 1)];

            glm::vec2 line = succ - prev;
            float len = glm::length(line);
            if (len < 1e-6f) continue;
            line /= len;
            glm::vec2 perp(-line.y, line.x);
            float dist = glm::abs(glm::dot(curr - prev, perp));

            if (dist > epsilon)
                next.push_back(curr);
        }
        next.push_back(simplified.back());
        simplified = next;
        epsilon *= 1.5f;
    }

    boundary = simplified;

    // ---- Step 4: Fan-triangulate from centroid with radial UVs ----
    glm::vec2 centroid(0.f);
    for (auto& p : boundary) centroid += p;
    centroid /= static_cast<float>(boundary.size());

    // Sort boundary counter-clockwise around centroid
    std::sort(boundary.begin(), boundary.end(),
              [&centroid](const glm::vec2& a, const glm::vec2& b) {
                  return math::AngleSigned(a - centroid)
                       < math::AngleSigned(b - centroid);
              });

    // Remove near-duplicate points
    std::vector<glm::vec2> clean;
    clean.reserve(boundary.size());
    for (auto& p : boundary)
    {
        if (clean.empty() || glm::length(p - clean.back()) > step * 0.1f)
            clean.push_back(p);
    }
    boundary = clean;

    if (boundary.size() < 3)
    {
        result = SpriteShape::MakeCircle(8);
        result.preset = SpriteShape::Preset::Custom;
        return result;
    }

    // vertices[0] = centroid
    result.vertices.push_back({ centroid, { 0.f, 0.f } });

    // All boundary vertices get r=1.0 -- the GPU interpolates linearly
    // from 0 (centroid) to 1 (boundary) across each fan triangle.
    for (auto& p : boundary)
    {
        glm::vec2 d = p - centroid;
        float angle = math::AngleToNormalized(glm::atan(d.y, d.x));
        result.vertices.push_back({ p, { 1.f, angle } });
    }

    int n = static_cast<int>(boundary.size());
    for (int i = 1; i <= n; ++i)
    {
        result.indices.push_back(0);
        result.indices.push_back(static_cast<unsigned int>(i));
        result.indices.push_back(static_cast<unsigned int>(i % n + 1));
    }

    return result;
}

} // namespace ettycc
