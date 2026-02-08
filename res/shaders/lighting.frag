#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uObjectColor;

void main() {
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightPos - FragPos);

    float diff = max(dot(norm, lightDir), 0.0);

    vec3 ambient = 0.15 * uLightColor;
    vec3 diffuse = diff * uLightColor;

    vec3 result = (ambient + diffuse) * uObjectColor;
    FragColor = vec4(result, 1.0);
}