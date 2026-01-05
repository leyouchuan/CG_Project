#include "skybox.h"

SkyBox::SkyBox(const std::vector<std::string>& textureFilenames) {
    GLfloat vertices[] = {-100.0f, 100.0f,  -100.0f, -100.0f, -100.0f, -100.0f, 100.0f,  -100.0f, -100.0f,
                          100.0f,  -100.0f, -100.0f, 100.0f,  100.0f,  -100.0f, -100.0f, 100.0f,  -100.0f,

                          -100.0f, -100.0f, 100.0f,  -100.0f, -100.0f, -100.0f, -100.0f, 100.0f,  -100.0f,
                          -100.0f, 100.0f,  -100.0f, -100.0f, 100.0f,  100.0f,  -100.0f, -100.0f, 100.0f,

                          100.0f,  -100.0f, -100.0f, 100.0f,  -100.0f, 100.0f,  100.0f,  100.0f,  100.0f,
                          100.0f,  100.0f,  100.0f,  100.0f,  100.0f,  -100.0f, 100.0f,  -100.0f, -100.0f,

                          -100.0f, -100.0f, 100.0f,  -100.0f, 100.0f,  100.0f,  100.0f,  100.0f,  100.0f,
                          100.0f,  100.0f,  100.0f,  100.0f,  -100.0f, 100.0f,  -100.0f, -100.0f, 100.0f,

                          -100.0f, 100.0f,  -100.0f, 100.0f,  100.0f,  -100.0f, 100.0f,  100.0f,  100.0f,
                          100.0f,  100.0f,  100.0f,  -100.0f, 100.0f,  100.0f,  -100.0f, 100.0f,  -100.0f,

                          -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, 100.0f,  100.0f,  -100.0f, -100.0f,
                          100.0f,  -100.0f, -100.0f, -100.0f, -100.0f, 100.0f,  100.0f,  -100.0f, 100.0f};

    // create vao and vbo
    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

    glBindVertexArray(0);

    try {
        // init texture
        _texture.reset(new ImageTextureCubemap(textureFilenames));

        const char* vsCode =
            "#version 330 core\n"
            "layout(location = 0) in vec3 aPosition;\n"
            "out vec3 texCoord;\n"
            "uniform mat4 projection;\n"
            "uniform mat4 view;\n"
            "\n"
            "// 优化的天空盒顶点着色器\n"
            "// 确保天空盒始终在背景渲染，无裁剪问题\n"
            "void main() {\n"
            "   // 直接使用顶点位置作为方向向量\n"
            "   texCoord = aPosition;\n"
            "   \n"
            "   // 创建只有旋转分量的视图矩阵\n"
            "   mat4 viewNoTranslation = mat4(mat3(view));\n"
            "   \n"
            "   // 计算裁剪空间位置\n"
            "   // 注意：aPosition 已经很大（100单位），确保覆盖整个场景\n"
            "   vec4 clipPos = projection * viewNoTranslation * vec4(aPosition, 1.0);\n"
            "   \n"
            "   // 使用深度值为w，确保天空盒始终在背景（深度为1.0）\n"
            "   gl_Position = clipPos;\n"
            "   gl_Position.z = gl_Position.w * 0.999999; // 略微小于1.0，确保在背景\n"
            "}\n";

        const char* fsCode =
            "#version 330 core\n"
            "out vec4 color;\n"
            "in vec3 texCoord;\n"
            "uniform samplerCube cubemap;\n"
            "\n"
            "// 冬季森林天空盒着色器\n"
            "// 寒冷、灰白色的冬季天空效果\n"
            "void main() {\n"
            "   // 标准化方向向量\n"
            "   vec3 sampleDir = normalize(texCoord);\n"
            "   \n"
            "   // 获取原始纹理颜色\n"
            "   vec3 texColor = texture(cubemap, sampleDir).rgb;\n"
            "   \n"
            "   // 冬季天空的颜色校正\n"
            "   // 增加蓝色和灰色调，模拟寒冷冬季天空\n"
            "   float winterBlueBoost = 1.15;  // 增加蓝色\n"
            "   float winterDesaturation = 0.8;  // 降低饱和度，增加灰色感\n"
            "   \n"
            "   // 计算灰度值\n"
            "   float gray = dot(texColor, vec3(0.299, 0.587, 0.114));\n"
            "   \n"
            "   // 应用冬季效果\n"
            "   vec3 winterColor = mix(vec3(gray), texColor, winterDesaturation);\n"
            "   winterColor.b *= winterBlueBoost;\n"
            "   winterColor.b = min(winterColor.b, 1.0);\n"
            "   \n"
            "   // 根据天空位置调整颜色\n"
            "   float height = sampleDir.y;  // -1 到 1\n"
            "   \n"
            "   // 天空高处：更蓝更亮\n"
            "   if (height > 0.3) {\n"
            "       float zenithFactor = smoothstep(0.3, 1.0, height);\n"
            "       vec3 zenithColor = vec3(0.75, 0.82, 0.95);  // 冬季高空蓝\n"
            "       winterColor = mix(winterColor, zenithColor, zenithFactor * 0.6);\n"
            "   }\n"
            "   \n"
            "   // 接近地平线：增加雾感和灰白色\n"
            "   float horizonFactor = 1.0 - abs(height);\n"
            "   if (horizonFactor > 0.7) {\n"
            "       float horizonBlend = smoothstep(0.7, 1.0, horizonFactor);\n"
            "       vec3 horizonFog = vec3(0.85, 0.88, 0.95);  // 冬季雾色\n"
            "       winterColor = mix(winterColor, horizonFog, horizonBlend * 0.5);\n"
            "   }\n"
            "   \n"
            "   // 边缘平滑处理（消除立方体感）\n"
            "   vec3 absDir = abs(sampleDir);\n"
            "   float maxAxis = max(absDir.x, max(absDir.y, absDir.z));\n"
            "   \n"
            "   if (maxAxis > 0.95) {\n"
            "       // 轻微多重采样，使边缘过渡更自然\n"
            "       vec3 sumColor = vec3(0.0);\n"
            "       float totalWeight = 0.0;\n"
            "       \n"
            "       for (int i = -1; i <= 1; i++) {\n"
            "           for (int j = -1; j <= 1; j++) {\n"
            "               vec3 offset = vec3(i * 0.0003, j * 0.0003, 0.0);\n"
            "               vec3 sampleColor = texture(cubemap, normalize(sampleDir + offset)).rgb;\n"
            "               float weight = exp(-float(i*i + j*j) / 2.0);\n"
            "               sumColor += sampleColor * weight;\n"
            "               totalWeight += weight;\n"
            "           }\n"
            "       }\n"
            "       \n"
            "       float edgeBlend = smoothstep(0.95, 0.99, maxAxis);\n"
            "       vec3 blendedColor = mix(texColor, sumColor / totalWeight, edgeBlend * 0.6);\n"
            "       \n"
            "       // 应用冬季效果到混合后的颜色\n"
            "       float grayBlended = dot(blendedColor, vec3(0.299, 0.587, 0.114));\n"
            "       vec3 winterBlended = mix(vec3(grayBlended), blendedColor, winterDesaturation);\n"
            "       winterBlended.b *= winterBlueBoost;\n"
            "       winterBlended.b = min(winterBlended.b, 1.0);\n"
            "       \n"
            "       winterColor = mix(winterColor, winterBlended, edgeBlend);\n"
            "   }\n"
            "   \n"
            "   // 最终亮度调整\n"
            "   float finalBrightness = mix(0.85, 1.1, smoothstep(-1.0, 1.0, height));\n"
            "   winterColor *= finalBrightness;\n"
            "   \n"
            "   // 轻微曝光调整\n"
            "   winterColor = 1.0 - exp(-winterColor * 1.1);\n"
            "   \n"
            "   // 确保颜色在合理范围内\n"
            "   winterColor = clamp(winterColor, 0.0, 1.0);\n"
            "   \n"
            "   color = vec4(winterColor, 1.0);\n"
            "}\n";
        //texCoord表示3D纹理坐标的方向向量，cubemap表示立方体贴图的纹理采样器
        _shader.reset(new GLSLProgram);
        _shader->attachVertexShader(vsCode);
        _shader->attachFragmentShader(fsCode);
        _shader->link();
    } catch (const std::exception&) {
        cleanup();
        throw;
    }

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::stringstream ss;
        ss << "skybox creation failure, (code " << error << ")";
        cleanup();
        throw std::runtime_error(ss.str());
    }
}

SkyBox::SkyBox(SkyBox&& rhs) noexcept
    : _vao(rhs._vao), _vbo(rhs._vbo), _texture(std::move(rhs._texture)),
      _shader(std::move(rhs._shader)) {
    rhs._vao = 0;
    rhs._vbo = 0;
}

SkyBox::~SkyBox() {
    cleanup();
}

void SkyBox::draw(const glm::mat4& projection, const glm::mat4& view) {
    // TODO:: draw skybox
    // write your code here
    // -----------------------------------------------
    // ...
    // -----------------------------------------------
    //天空盒着色器
    _shader->use();
    // 2. 去除平移分量的 view（只用旋转）
    glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
    _shader->setUniformMat4("projection", projection);
    _shader->setUniformMat4("view", viewNoTranslation);

    // 3. 设置深度状态（不写深度，但允许测试）
    GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLint prevDepthFunc = 0;
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    // 4. 绑定纹理到单元 0
    const int texUnit = 0;
    _texture->bind(texUnit);
    _shader->setUniformInt("cubemap", texUnit);

    // 5. 绑定 VAO 并绘制
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    // 6. 恢复深度写入与深度函数
    glDepthMask(GL_TRUE);
    glDepthFunc(prevDepthFunc);
    if (!depthTestWasEnabled) glDisable(GL_DEPTH_TEST);
}

void SkyBox::cleanup() {
    if (_vbo != 0) {
        glDeleteBuffers(1, &_vbo);
        _vbo = 0;
    }

    if (_vao != 0) {
        glDeleteVertexArrays(1, &_vao);
        _vao = 0;
    }
}