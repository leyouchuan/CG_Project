#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D ssao;
uniform sampler2D footprintMap;  // 保留脚印纹理

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 lightColor;

uniform float ambientStrength;
uniform vec3 materialSpecular;
uniform float materialShininess;
uniform float time;  // 保留time用于避免uniform被优化

// 脚印参数
uniform vec3 footprintColor;
uniform float footprintRadius;
uniform float footprintRoughness;

void main() {
    vec3 pos = texture(gPosition, TexCoords).rgb;
    vec3 normal = normalize(texture(gNormal, TexCoords).rgb);
    vec3 albedo = texture(gAlbedo, TexCoords).rgb;
    float occlusion = texture(ssao, TexCoords).r;
    
    // ========== 原版光照计算 ==========
    // ambient with ssao
    vec3 ambient = ambientStrength * albedo * occlusion;
    
    // diffuse
    vec3 L = normalize(lightPos - pos);
    float diff = max(dot(normal, L), 0.0);
    vec3 diffuse = diff * albedo * lightColor;
    
    // specular
    vec3 V = normalize(viewPos - pos);
    vec3 H = normalize(L + V);
    float spec = 0.0;
    if (diff > 0.0) {
        spec = pow(max(dot(normal, H), 0.0), materialShininess);
    }
    vec3 specular = spec * materialSpecular * lightColor;
    
    // 组合基础颜色
    vec3 color = ambient + diffuse + specular;
    
    // ========== 添加脚印效果（只在地面Y≈-2.0附近）==========
    if (abs(pos.y + 2.0) < 0.2) {  // 地面判断
        // 转换为脚印纹理UV坐标
        vec2 footprintUV = (pos.xz + 50.0) / 100.0;
        footprintUV = clamp(footprintUV, 0.01, 0.99);
        
        vec4 footprint = texture(footprintMap, footprintUV);
        
        if (footprint.a > 0.01) {
            // 使用roughness控制混合强度
            float roughnessEffect = mix(0.2, 0.5, footprintRoughness);
            float colorMix = footprint.a * roughnessEffect;
            
            // 混合脚印颜色
            color = mix(color, footprintColor, colorMix);
        }
    }
    
    FragColor = vec4(color, 1.0);
    
    // 防止uniform被优化掉（乘以0不影响结果）
    FragColor.rgb *= (1.0 + time * 0.0);
    FragColor.rgb *= (1.0 + footprintRadius * 0.0);
}