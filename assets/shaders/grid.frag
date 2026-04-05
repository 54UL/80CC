#version 330 core

in  vec3 worldPos;
out vec4 FragColor;

uniform vec2  camPos;
uniform float viewSize; // visible area diagonal — drives fade distance

// Returns [0,1] line intensity at coord for the given period and half-thickness
float gridLine(float coord, float period, float thickness)
{
    float d  = abs(mod(coord + period * 0.5, period) - period * 0.5);
    float fw = max(fwidth(coord), 1e-4);
    return 1.0 - smoothstep(thickness - fw, thickness + fw, d);
}

// Intensity for a line centred on coord=0
float axisLine(float coord, float thickness)
{
    float fw = max(fwidth(coord), 1e-4);
    return 1.0 - smoothstep(thickness - fw, thickness + fw, abs(coord));
}

void main()
{
    float dist = length(worldPos.xy - camPos);

    // Fade distance scales with the visible area so the grid never clips
    float fadeStart = viewSize * 0.5;
    float fadeEnd   = viewSize * 0.95;
    float fade = 1.0 - smoothstep(fadeStart, fadeEnd, dist);
    if (fade <= 0.0) discard;

    // ── Minor grid (every 1 unit) ────────────────────────────────────
    float minorX = gridLine(worldPos.x, 1.0, 0.012);
    float minorY = gridLine(worldPos.y, 1.0, 0.012);
    float minor  = max(minorX, minorY);

    // ── Major grid (every 10 units — ruler marks) ────────────────────
    float majorX = gridLine(worldPos.x, 10.0, 0.028);
    float majorY = gridLine(worldPos.y, 10.0, 0.028);
    float major  = max(majorX, majorY);

    // soft glow around major lines
    float majorGlowX = gridLine(worldPos.x, 10.0, 0.18) * 0.35;
    float majorGlowY = gridLine(worldPos.y, 10.0, 0.18) * 0.35;
    float majorGlow  = max(majorGlowX, majorGlowY);

    // ── World axes ───────────────────────────────────────────────────
    float axX     = axisLine(worldPos.y, 0.030);  // Y=0  →  X axis
    float axY     = axisLine(worldPos.x, 0.030);  // X=0  →  Y axis
    float axGlowX = axisLine(worldPos.y, 0.18) * 0.45;
    float axGlowY = axisLine(worldPos.x, 0.18) * 0.45;

    // ── Compose colour ───────────────────────────────────────────────
    vec3  col   = vec3(0.0);
    float alpha = 0.0;

    // minor
    col   += vec3(0.26, 0.26, 0.28) * minor;
    alpha  = max(alpha, minor * 0.65);

    // major (blueish-white + glow)
    col    = mix(col, vec3(0.60, 0.62, 0.85), major);
    alpha  = max(alpha, major * 0.90);
    col   += vec3(0.18, 0.20, 0.60) * majorGlow;
    alpha  = max(alpha, majorGlow * 0.50);

    // X axis (red + glow)
    col    = mix(col, vec3(0.95, 0.28, 0.28), axX);
    alpha  = max(alpha, axX * 0.95);
    col   += vec3(0.80, 0.08, 0.08) * axGlowX;
    alpha  = max(alpha, axGlowX * 0.55);

    // Y axis (green + glow)
    col    = mix(col, vec3(0.28, 0.95, 0.28), axY);
    alpha  = max(alpha, axY * 0.95);
    col   += vec3(0.08, 0.70, 0.08) * axGlowY;
    alpha  = max(alpha, axGlowY * 0.55);

    alpha *= fade;
    if (alpha < 0.005) discard;

    FragColor = vec4(col, alpha);
}
