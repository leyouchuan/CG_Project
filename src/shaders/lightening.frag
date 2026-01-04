#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D ssao;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 lightColor;

uniform float ambientStrength;
uniform vec3 materialSpecular;
uniform float materialShininess;

void main() {
    vec3 pos = texture(gPosition, TexCoords).rgb;
    vec3 normal = normalize(texture(gNormal, TexCoords).rgb);
    vec3 albedo = texture(gAlbedo, TexCoords).rgb;
    float occlusion = texture(ssao, TexCoords).r;

    // ambient with ssao
    vec3 ambient = ambientStrength * albedo * occlusion;

    // diffuse
    vec3 L = normalize(lightPos - pos);
    float diff = max(dot(normal, L), 0.0);
    vec3 diffuse = diff * albedo * lightColor;

    // specular (Blinn-Phong)
    vec3 V = normalize(viewPos - pos);
    vec3 H = normalize(L + V);
    float spec = 0.0;
    if (diff > 0.0) spec = pow(max(dot(normal, H), 0.0), materialShininess);
    vec3 specular = spec * materialSpecular * lightColor;

    vec3 color = ambient + diffuse + specular;
    FragColor = vec4(color, 1.0);
}