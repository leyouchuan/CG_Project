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
    float starFactor = 0.0;
    if (albedo.r > 0.7 && albedo.g > 0.7 && albedo.b > 0.2) {
        starFactor = 1.0;
        float pulse = 0.6 + 0.4 * sin(time * 5.0);
        albedo *= (1.0 + pulse * 0.5);
        float colorShift = sin(time * 3.0) * 0.2;
        albedo.r += colorShift * 0.1;
        albedo.g += colorShift * 0.05;
        albedo.b -= colorShift * 0.15;
    }
    
    // 即使不是星星，也添加一个微小的基于时间的扰动，确保time不被优化掉
    albedo += vec3(sin(time * 0.001) * 0.0001);
    
    // ★★★ 终极修复：让time直接影响输出 ★★★
    gAlbedo = albedo;
    
    // 方法1：直接乘以(1.0 + 0)
    gAlbedo *= (1.0 + time * 0.0);
    
    // 方法2：永远为false的条件
    if (time < -999999.0) {
        gAlbedo = vec3(1.0);
    }
}
