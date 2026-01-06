#version 330 core
out float FragColor;
in vec2 TexCoords;
uniform sampler2D ssaoInput;
void main() {
    float result = 0.0;
    // 3x3 
    float kernel[9] = float[](
        1,2,1,
        2,4,2,
        1,2,1
    );
    float sum = 16.0;
    ivec2 texSize = textureSize(ssaoInput, 0);
    vec2 texel = 1.0 / vec2(texSize);
    int idx = 0;
    for(int y=-1;y<=1;++y){
        for(int x=-1;x<=1;++x){
            vec2 off = vec2(x,y) * texel;
            result += texture(ssaoInput, TexCoords + off).r * kernel[idx++];
        }
    }
    FragColor = result / sum;
}
