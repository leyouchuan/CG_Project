#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];

uniform mat4 projection;
uniform float radius;
uniform float bias;

uniform vec2 noiseScale;

void main() {
    vec3 fragPos = texture(gPosition, TexCoords).rgb;
    vec3 normal = normalize(texture(gNormal, TexCoords).rgb);

    vec3 randomVec = normalize(texture(texNoise, TexCoords * noiseScale).xyz);

    // TBN
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for(int i = 0; i < 64; ++i) {
        vec3 sample = TBN * samples[i]; // in world space (since samples are hemisphere)
        sample = fragPos + sample * radius;

        vec4 offset = projection * vec4(sample, 1.0);
        offset.xyz /= offset.w;
        vec2 sampleUV = offset.xy * 0.5 + 0.5;

        vec3 samplePos = texture(gPosition, sampleUV).rgb;
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - samplePos.z + 1e-5));
        if (samplePos.z >= sample.z + bias) {
            occlusion += rangeCheck;
        }
    }
    occlusion = 1.0 - (occlusion / 64.0);
    FragColor = occlusion;
}
