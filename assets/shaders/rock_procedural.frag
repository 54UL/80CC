#version 330 core

out vec4 FragColor;

in vec2 vUV;
in vec2 vLocalPos;

uniform float time;
uniform vec3  uBaseColor;
uniform vec3  uEdgeColor;
uniform float uEdgeWidth;

// --- Noise primitives -------------------------------------------------------

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p, int octaves)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < octaves; ++i)
    {
        value += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

float warpedFbm(vec2 p)
{
    vec2 q = vec2(fbm(p, 4), fbm(p + vec2(5.2, 1.3), 4));
    return fbm(p + 2.0 * q, 4);
}

float veins(vec2 p)
{
    float v = 0.0;
    float amp = 1.0;
    for (int i = 0; i < 3; ++i)
    {
        float n = noise(p);
        v += amp * abs(2.0 * n - 1.0);
        p *= 2.2;
        amp *= 0.4;
    }
    return 1.0 - v;
}

void main()
{
    float dist = vUV.x;

    float edgeWidth = max(uEdgeWidth, 0.05);
    float edgeFactor = smoothstep(1.0 - edgeWidth, 1.0, dist);

    vec2 noiseCoord = vLocalPos * 6.0;

    float grain = warpedFbm(noiseCoord);
    float variation = grain * 0.2 - 0.1;

    float fineGrain = noise(noiseCoord * 12.0) * 0.06 - 0.03;

    float veinPattern = veins(noiseCoord * 3.0);
    float veinDarken = smoothstep(0.3, 0.5, veinPattern) * 0.08;

    vec3 interior = uBaseColor + variation + fineGrain - veinDarken;

    float edgeNoise = noise(noiseCoord * 4.0) * 0.05;
    vec3 edge = uEdgeColor + edgeNoise;

    vec3 color = mix(interior, edge, edgeFactor);

    float centerDarken = smoothstep(0.0, 0.35, dist) * 0.06;
    color -= centerDarken;

    float rimLight = smoothstep(0.85, 0.95, dist) * 0.08;
    color += rimLight;

    // Black outline border at the shape edge
    float outlineOuter = smoothstep(0.92, 0.88, dist);  // fade in from outside
    float outlineInner = smoothstep(0.78, 0.82, dist);  // fade in from inside
    float outline = outlineOuter * outlineInner;
    color = mix(color, vec3(0.0), outline * 0.85);

    float alpha = smoothstep(1.02, 0.96, dist);

    FragColor = vec4(color, alpha);
}
