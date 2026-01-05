#version 330 core
layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec3 gAlbedo;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 fallbackColor;
uniform bool useAlbedoTexture;
uniform sampler2D albedoTex;
uniform float time;  // 用于星星特效的时间

void main() {
    gPosition = FragPos;
    gNormal = normalize(Normal);
    vec3 albedo = fallbackColor;
    if(useAlbedoTexture) {
        albedo = texture(albedoTex, TexCoords).rgb;
    }
    
    // 检测并增强星星的发光效果
    // 星星通常有高亮度和特定颜色范围
    float starFactor = 0.0;
    if (albedo.r > 0.7 && albedo.g > 0.7 && albedo.b > 0.2) {
        starFactor = 1.0;
        
        // 根据时间增加星星的亮度变化
        float pulse = 0.6 + 0.4 * sin(time * 5.0);
        albedo *= (1.0 + pulse * 0.5);
        
        // 添加颜色变化，让星星更加闪烁
        float colorShift = sin(time * 3.0) * 0.2;
        albedo.r += colorShift * 0.1;
        albedo.g += colorShift * 0.05;
        albedo.b -= colorShift * 0.15;
    }
    
    gAlbedo = albedo;
}