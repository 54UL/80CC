#version 330 core

flat in vec3 vIdColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vIdColor, 1.0);
}
