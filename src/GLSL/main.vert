#version 330

layout(location = 0) in vec3 VertexPosition;
//layout (location = 1) in vec2 TexCoords;
layout(location =2 ) in vec3 VertexNormals;

out vec3 Tc;       // cordenadas a pasar para el fragment
uniform  mat4 PVM; // matriz

void main()
{
    //gl_Position = ModelMatrix * ViewMatrix * ProjectionMatrix * vec4 (VertexPosition,1);

    gl_Position =  PVM * vec4 (VertexPosition,1);
    Tc = VertexNormals;
}