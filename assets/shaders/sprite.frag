#version 330 core

out vec4 FragColor;
in vec2 TexCoord;

uniform float time;
uniform sampler2D ourTexture;

// Tiling multiplier: (1,1) = no tiling change.
// Set to the sprite's world-space scale to keep texture density constant.
uniform vec2 tiling;

void main()
{
    FragColor = texture(ourTexture, vec2(TexCoord.x, 1.0 - TexCoord.y) * tiling);
}
