#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform float time;  // 声明时间uniform以在片段着色器中使用

out vec3 FragPos;   // world space
out vec3 Normal;    // world space
out vec2 TexCoords;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = normalize(normalMatrix * aNormal);
    TexCoords = aTexCoords;
    gl_Position = projection * view * worldPos;
}