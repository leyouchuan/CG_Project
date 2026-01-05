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
uniform float time;  // 全局时间，用于动态效果

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
    
    // 检测星星：星星通常有高亮度和特定颜色
    // 计算星星的发光特效
    float starGlow = 0.0;
    float starDetect = 0.0;
    
    // 检测亮黄色/白色高亮度物体（星星）
    if (albedo.r > 0.8 && albedo.g > 0.8 && albedo.b > 0.2) {
        starDetect = 1.0;
        
        // 根据时间创建脉冲效果
        float pulse1 = 0.5 + 0.5 * sin(time * 4.0);
        float pulse2 = 0.5 + 0.5 * sin(time * 6.0 + 1.0);
        float pulse3 = 0.5 + 0.5 * sin(time * 8.0 + 2.0);
        
        // 核心发光效果
        starGlow = 0.8 * pulse1 + 0.5 * pulse2 + 0.3 * pulse3;
        
        // 添加发光光晕
        float distanceFromViewer = distance(pos, viewPos);
        float glowFalloff = 1.0 / (1.0 + distanceFromViewer * 0.2);
        
        // 创建星星颜色变化
        vec3 starColor = vec3(
            1.0 + 0.2 * sin(time * 3.0),
            0.95 + 0.15 * sin(time * 3.5 + 1.0),
            0.3 + 0.25 * sin(time * 2.5 + 2.0)
        );
        
        // 将星星颜色混合到最终输出
        color = mix(color, starColor * (1.0 + starGlow), 0.6 * starDetect * glowFalloff);
        
        // 添加闪烁的光点
        float sparkle = pow(sin(time * 12.0 + pos.x * 10.0 + pos.z * 10.0) * 0.5 + 0.5, 10.0);
        color += vec3(1.0, 1.0, 0.8) * sparkle * 0.4 * glowFalloff;
    }
    
    // 增强星星的自发光效果
    float selfIllumination = max(albedo.r, max(albedo.g, albedo.b)) * starDetect;
    color += albedo * selfIllumination * 2.0;
    
    FragColor = vec4(color, 1.0);
}