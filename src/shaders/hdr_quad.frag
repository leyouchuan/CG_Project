#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D hdrBuffer;
uniform sampler2D gPosition;   // ★新增
uniform float exposure;
uniform float gamma;

void main()
{
    // ★新增：背景像素在 GBuffer 里没有写入位置，通常为 (0,0,0)
    vec3 pos = texture(gPosition, TexCoords).xyz;
    if (abs(pos.x) + abs(pos.y) + abs(pos.z) < 1e-4)
        discard;

    vec3 hdr = texture(hdrBuffer, TexCoords).rgb;

    vec3 mapped = vec3(1.0) - exp(-hdr * exposure);
    mapped = pow(mapped, vec3(1.0 / gamma));
    FragColor = vec4(mapped, 1.0);
}