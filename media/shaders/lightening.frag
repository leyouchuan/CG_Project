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

// 雪的材质特性
const vec3 snowSpecular = vec3(0.25, 0.25, 0.3);   // 雪的镜面反射颜色（冷色调）
const float snowShininess = 128.0;                 // 雪的光泽度（雪比较闪亮）
const float snowAmbientBoost = 1.8;                // 雪的环境光增强
const vec3 snowShadowColor = vec3(0.85, 0.88, 1.0); // 雪的阴影颜色（蓝色调）

// 检测是否为地面（基于Y坐标）
bool isSnowGround(vec3 position) {
    // 地面在Y = -2.1附近（groundY - 0.1f）
    return abs(position.y + 2.1) < 0.2;
}

// 简单的噪声函数，用于创建雪的细节
float snowNoise(vec2 uv) {
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

// 雪的表面细节函数
float snowSurfaceDetail(vec3 position, vec3 normal) {
    // 基于位置创建简单的噪点图案
    vec2 uv = position.xz * 0.1;
    float noise = snowNoise(uv);
    
    // 混合多个噪声频率
    noise += snowNoise(uv * 2.0) * 0.5;
    noise += snowNoise(uv * 4.0) * 0.25;
    
    return noise * 0.1; // 轻微的表面变化
}

void main() {
    vec3 pos = texture(gPosition, TexCoords).rgb;
    vec3 normal = normalize(texture(gNormal, TexCoords).rgb);
    vec3 albedo = texture(gAlbedo, TexCoords).rgb;
    float occlusion = texture(ssao, TexCoords).r;

    // 判断是否为雪地
    bool isSnow = isSnowGround(pos);
    
    if (isSnow) {
        // 雪的增强效果
        
        // 1. 调整基础颜色
        albedo = vec3(0.98, 0.98, 1.0); // 雪的冷白色
        
        // 2. 添加表面细节
        float surfaceDetail = snowSurfaceDetail(pos, normal);
        albedo *= (1.0 + surfaceDetail * 0.2);
        
        // 3. 增强环境光（雪反射光很好）
        float ambient = ambientStrength * snowAmbientBoost * occlusion;
        
        // 4. 计算漫反射
        vec3 L = normalize(lightPos - pos);
        float diff = max(dot(normal, L), 0.0);
        
        // 5. 雪的阴影颜色处理
        vec3 shadowColor = mix(albedo, snowShadowColor, (1.0 - diff) * 0.3);
        vec3 diffuse = diff * shadowColor * lightColor;
        
        // 6. 镜面反射（雪有轻微的高光）
        vec3 V = normalize(viewPos - pos);
        vec3 H = normalize(L + V);
        float spec = 0.0;
        if (diff > 0.0) {
            spec = pow(max(dot(normal, H), 0.0), snowShininess);
        }
        vec3 specular = spec * snowSpecular * lightColor;
        
        // 7. 组合颜色
        vec3 color = ambient * albedo + diffuse + specular;
        
        // 8. 轻微的颜色变化，使雪看起来更自然
        float colorVariation = sin(pos.x * 0.05 + pos.z * 0.07) * 0.02;
        color += vec3(colorVariation * 0.8, colorVariation * 0.8, colorVariation);
        
        FragColor = vec4(min(color, vec3(1.0)), 1.0);
    } else {
        // 其他物体的标准渲染
        vec3 ambient = ambientStrength * albedo * occlusion;
        
        vec3 L = normalize(lightPos - pos);
        float diff = max(dot(normal, L), 0.0);
        vec3 diffuse = diff * albedo * lightColor;
        
        vec3 V = normalize(viewPos - pos);
        vec3 H = normalize(L + V);
        float spec = 0.0;
        if (diff > 0.0) spec = pow(max(dot(normal, H), 0.0), materialShininess);
        vec3 specular = spec * materialSpecular * lightColor;
        
        vec3 color = ambient + diffuse + specular;
        FragColor = vec4(color, 1.0);
    }
}