#version 330 core

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;

// Tiling multiplier: (1,1) = no tiling change.
// Increase to tile the texture more densely across the soft body mesh.
uniform vec2 tiling;

void main()
{
    FragColor = texture(ourTexture, vec2(TexCoord.x, 1.0 - TexCoord.y) * tiling);
}
