#version 330 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScene;

// existing
uniform float uBrightness; // 0 = no change
uniform float uContrast;   // 1 = no change

// new
uniform float uExposure;   // 0 = no change (stops)
uniform float uSaturation; // 1 = no change
uniform float uVignette;   // 0 = off, 1 = strong
uniform float uVignetteSoftness; // 0.1..0.8 typical

void main() {
    vec3 color = texture(uScene, vUV).rgb;

    // --- exposure (photographic) ---
    // exposure in "stops": +1 doubles brightness, -1 halves
    color *= exp2(uExposure);

    // --- brightness ---
    color += vec3(uBrightness);

    // --- contrast (pivot around 0.5) ---
    color = (color - 0.5) * uContrast + 0.5;

    // --- saturation ---
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 gray = vec3(luma);
    color = mix(gray, color, uSaturation);

    // --- vignette ---
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(vUV, center); // 0 at center, ~0.707 at corner
    // smooth darkening curve
    float vig = smoothstep(0.707 - uVignetteSoftness, 0.707, dist);
    color *= (1.0 - uVignette * vig);

    FragColor = vec4(color, 1.0);
}
