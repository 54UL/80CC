#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
uniform float time;

uniform sampler2D ourTexture;
float accumulatedTime = 0.0;


void main()
{
    accumulatedTime += time;
    // vec2 yFlipped = vec2(TexCoord.x, 1 - TexCoord.y);
    // FragColor = texture(ourTexture, yFlipped);

    // Get the original texture color
    vec4 originalColor = texture(ourTexture, vec2(TexCoord.x,(1-TexCoord.y)));

    // Add a sine wave effect to the y-coordinate
    float frequency = 5.0; // Adjust the frequency of the sine wave
    float amplitude = 0.1; // Adjust the amplitude of the sine wave
    float wave = amplitude * sin(2.0 * 3.14159 * frequency * TexCoord.x + accumulatedTime) + cos(1 * 3.14159 * frequency * TexCoord.y + accumulatedTime*10);

    // Apply the sine wave effect to the y-coordinate
    vec2 distortedTexCoord = vec2(TexCoord.x, (1-TexCoord.y) + wave);

    // Sample the texture with the distorted texture coordinates
    vec4 distortedColor = texture(ourTexture, distortedTexCoord);

    // Blend the original and distorted colors
    // FragColor = mix(originalColor, distortedColor, 0.5);
    FragColor = distortedColor;
}