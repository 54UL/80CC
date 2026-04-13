#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

// Per-instance attributes (mat4 occupies locations 2-5)
layout (location = 2) in mat4 iModel;
layout (location = 6) in vec2 iTiling;

uniform mat4 uPV; // Projection * View (shared across all instances)

out vec2 TexCoord;
out vec2 vTiling;

void main()
{
    gl_Position = uPV * iModel * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    vTiling = iTiling;
}
