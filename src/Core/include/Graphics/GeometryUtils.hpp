#pragma once

#include <Math/Constants.hpp>
#include <Math/Utils.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>
#include <cmath>

namespace ettycc::geo
{
    // -- Point-to-edge distance ------------------------------------------------
    // Returns the shortest distance from point p to line segment [a, b].
    // outT receives the parameter along the segment (0 = a, 1 = b).
    inline float PointToEdgeDist(glm::vec2 p, glm::vec2 a, glm::vec2 b, float& outT)
    {
        glm::vec2 ab = b - a;
        float lenSq = glm::dot(ab, ab);
        if (lenSq < 1e-8f) { outT = 0.f; return glm::length(p - a); }
        outT = glm::clamp(glm::dot(p - a, ab) / lenSq, 0.f, 1.f);
        return glm::length(p - (a + ab * outT));
    }

    inline float PointToEdgeDist(glm::vec2 p, glm::vec2 a, glm::vec2 b)
    {
        float t;
        return PointToEdgeDist(p, a, b, t);
    }

    // -- Edge normal / tangent -------------------------------------------------
    inline glm::vec2 EdgeNormal(glm::vec2 a, glm::vec2 b)
    {
        glm::vec2 dir = b - a;
        if (glm::length(dir) < 1e-6f) return { 0.f, 1.f };
        return glm::normalize(glm::vec2(-dir.y, dir.x));
    }

    inline glm::vec2 EdgeTangent(glm::vec2 a, glm::vec2 b)
    {
        glm::vec2 dir = b - a;
        if (glm::length(dir) < 1e-6f) return { 1.f, 0.f };
        return glm::normalize(dir);
    }

    // -- 2D cross product (signed area of parallelogram) ----------------------
    // Delegates to math::Cross2D for the implementation.
    inline float Cross2D(glm::vec2 O, glm::vec2 A, glm::vec2 B)
    {
        return math::Cross2D(O, A, B);
    }

    // -- Convex Hull (Andrew's monotone chain) --------------------------------
    // Input:  a set of 2D points (will be sorted in-place).
    // Output: convex hull vertices in counter-clockwise order.
    inline std::vector<glm::vec2> ConvexHull(std::vector<glm::vec2>& points)
    {
        // Sort lexicographically
        std::sort(points.begin(), points.end(), [](const glm::vec2& a, const glm::vec2& b) {
            return a.x < b.x || (a.x == b.x && a.y < b.y);
        });

        // Remove duplicates
        points.erase(std::unique(points.begin(), points.end(),
            [](const glm::vec2& a, const glm::vec2& b) {
                return math::NearEqual(a, b, 1e-5f);
            }), points.end());

        if (points.size() < 3) return points;

        std::vector<glm::vec2> hull;
        hull.reserve(points.size() * 2);

        // Lower hull
        for (auto& p : points) {
            while (hull.size() >= 2 && Cross2D(hull[hull.size()-2], hull[hull.size()-1], p) <= 0.f)
                hull.pop_back();
            hull.push_back(p);
        }

        // Upper hull
        size_t lower_size = hull.size() + 1;
        for (int i = static_cast<int>(points.size()) - 2; i >= 0; --i) {
            while (hull.size() >= lower_size && Cross2D(hull[hull.size()-2], hull[hull.size()-1], points[i]) <= 0.f)
                hull.pop_back();
            hull.push_back(points[i]);
        }
        hull.pop_back(); // remove last (duplicate of first)

        return hull;
    }

    // -- Fan triangulation from centroid --------------------------------------
    // Takes a convex polygon (ordered boundary vertices) and produces
    // a triangle fan with the centroid as hub vertex.
    // Returns (vertices_with_centroid_at_0, indices).
    struct FanResult {
        std::vector<glm::vec2> vertices; // [0] = centroid, [1..N] = boundary
        std::vector<unsigned int> indices;
    };

    inline FanResult FanTriangulate(const std::vector<glm::vec2>& boundary)
    {
        FanResult result;
        if (boundary.size() < 3) return result;

        // Compute centroid
        glm::vec2 centroid(0.f);
        for (auto& p : boundary) centroid += p;
        centroid /= static_cast<float>(boundary.size());

        result.vertices.push_back(centroid);
        for (auto& p : boundary)
            result.vertices.push_back(p);

        int n = static_cast<int>(boundary.size());
        for (int i = 1; i <= n; ++i)
        {
            result.indices.push_back(0);
            result.indices.push_back(static_cast<unsigned int>(i));
            result.indices.push_back(static_cast<unsigned int>(i % n + 1));
        }

        return result;
    }

    // -- Radial UV encoding ---------------------------------------------------
    // For procedural rendering: encodes UV based on distance from centroid.
    //   u = 1.0 for all boundary vertices (GPU interpolates 0→1 across triangles)
    //   v = normalized angle [0, 1]
    // Works on fan-triangulated shapes where vertices[0] = centroid.
    inline void ComputeRadialUVs(std::vector<glm::vec2>& positions,
                                  std::vector<glm::vec2>& uvs)
    {
        if (positions.empty()) return;
        uvs.resize(positions.size());

        glm::vec2 centroid = positions[0]; // first vertex is centroid in fan layout

        uvs[0] = { 0.f, 0.f }; // centroid
        for (size_t i = 1; i < positions.size(); ++i)
        {
            glm::vec2 d = positions[i] - centroid;
            float angle = math::AngleToNormalized(glm::atan(d.y, d.x));
            uvs[i] = { 1.f, angle };
        }
    }

    // -- Bounding circle radius -----------------------------------------------
    // Returns the radius of the smallest enclosing circle centered at centroid.
    inline float BoundingRadius(const std::vector<glm::vec2>& points, glm::vec2 centroid)
    {
        float maxR = 0.f;
        for (auto& p : points)
            maxR = glm::max(maxR, glm::length(p - centroid));
        return maxR;
    }

} // namespace ettycc::geo
