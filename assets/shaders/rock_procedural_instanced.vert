#version 330 core

// Mega-draw layout: positions are pre-transformed to world space,
// material params and local coords baked per-vertex.
layout (location = 0) in vec3 aPos;        // world-space position
layout (location = 1) in vec2 aTexCoord;   // radial UV
layout (location = 2) in vec2 aLocalPos;   // local-space position (for noise)
layout (location = 3) in vec3 aBaseColor;  // per-vertex base color
layout (location = 4) in vec4 aEdgeData;   // xyz = edge color, w = edge width

uniform mat4 uPV; // Projection * View

out vec2 vUV;
out vec2 vLocalPos;
out vec3 vBaseColor;
out vec3 vEdgeColor;
out float vEdgeWidth;

void main()
{
    gl_Position = uPV * vec4(aPos, 1.0);
    vUV = aTexCoord;
    vLocalPos = aLocalPos;
    vBaseColor = aBaseColor;
    vEdgeColor = aEdgeData.xyz;
    vEdgeWidth = aEdgeData.w;
}
