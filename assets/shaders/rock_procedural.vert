#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vUV;       // radial UV: x = dist from center [0,1], y = angle [0,1]
out vec2 vLocalPos; // local space position for noise

uniform mat4 PVM;

void main()
{
    gl_Position = PVM * vec4(aPos, 1.0);
    vUV = aTexCoord;
    vLocalPos = aPos.xy;
}
