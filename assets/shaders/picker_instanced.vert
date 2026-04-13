#version 330 core

layout (location = 0) in vec3 aPos;

// Per-instance attributes (mat4 occupies locations 2-5)
layout (location = 2) in mat4 iModel;
layout (location = 6) in vec3 iIdColor;

uniform mat4 uPV; // Projection * View (shared across all instances)

flat out vec3 vIdColor;

void main()
{
    gl_Position = uPV * iModel * vec4(aPos, 1.0);
    vIdColor = iIdColor;
}
