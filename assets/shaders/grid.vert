#version 330 core

layout(location = 0) in vec3 aPos;

out vec3 worldPos;

uniform mat4 PVM;
uniform mat4 model;

void main()
{
    vec4 wp    = model * vec4(aPos, 1.0);
    worldPos   = wp.xyz;
    gl_Position = PVM * vec4(aPos, 1.0);
}
