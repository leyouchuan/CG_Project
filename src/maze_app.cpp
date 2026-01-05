#include "maze_app.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <cstdlib>
#include <cstdio>
#include <direct.h>
#include <sstream>
#include <iomanip>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

void printCwd() {
    char buf[1024];
    if (getcwd(buf, sizeof(buf)) != nullptr) {
        std::cerr << "CWD: " << buf << std::endl;
    }
    else {
        std::perror("getcwd");
    }
}

static const std::string gbufferVs = "shaders/gbuffer.vert";
static const std::string gbufferFs = "shaders/gbuffer.frag";
static const std::string quadVs = "shaders/quad.vert";
static const std::string ssaoFs = "shaders/ssao.frag";
static const std::string ssaoBlurFs = "shaders/ssao_blur.frag";
static const std::string lightFs = "shaders/lightening.frag";
static const std::string hdrFs = "shaders/hdr_quad.frag";

void MazeApp::initResources() {
    printCwd();
    try {
        _gBufferShader = std::make_unique<GLSLProgram>();
        _gBufferShader->attachVertexShaderFromFile(getAssetFullPath(gbufferVs));
        _gBufferShader->attachFragmentShaderFromFile(getAssetFullPath(gbufferFs));
        _gBufferShader->link();
        std::cerr << "Loaded shader: " << gbufferVs << " + " << gbufferFs << std::endl;
        _gBufferShader->use();
        _gBufferShader->setUniformInt("albedoTex", 0);

        _ssaoShader = std::make_unique<GLSLProgram>();
        _ssaoShader->attachVertexShaderFromFile(getAssetFullPath(quadVs));
        _ssaoShader->attachFragmentShaderFromFile(getAssetFullPath(ssaoFs));
        _ssaoShader->link();
        std::cerr << "Loaded shader: " << quadVs << " + " << ssaoFs << std::endl;

        _ssaoBlurShader = std::make_unique<GLSLProgram>();
        _ssaoBlurShader->attachVertexShaderFromFile(getAssetFullPath(quadVs));
        _ssaoBlurShader->attachFragmentShaderFromFile(getAssetFullPath(ssaoBlurFs));
        _ssaoBlurShader->link();
        std::cerr << "Loaded shader: " << quadVs << " + " << ssaoBlurFs << std::endl;

        _lightingShader = std::make_unique<GLSLProgram>();
        _lightingShader->attachVertexShaderFromFile(getAssetFullPath(quadVs));
        _lightingShader->attachFragmentShaderFromFile(getAssetFullPath(lightFs));
        _lightingShader->link();

        _hdrShader = std::make_unique<GLSLProgram>();
        _hdrShader->attachVertexShaderFromFile(getAssetFullPath(quadVs));
        _hdrShader->attachFragmentShaderFromFile(getAssetFullPath(hdrFs));
        _hdrShader->link();
        std::cerr << "Loaded shader: " << quadVs << " + " << hdrFs << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "initResources failed: " << e.what() << std::endl;
    }
}

void MazeApp::createGBuffer() {
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, _windowWidth, _windowHeight, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, _windowWidth, _windowHeight, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _windowWidth, _windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);

    GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, _windowWidth, _windowHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "GBuffer Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MazeApp::createSSAOBuffer() {
    glGenFramebuffers(1, &ssaoFBO);
    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, _windowWidth, _windowHeight, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "SSAO FBO incomplete\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffers(1, &ssaoBlurFBO);
    glGenTextures(1, &ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, _windowWidth, _windowHeight, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffers(1, &hdrFBO);
    glGenTextures(1, &hdrColorBuffer);
    glBindTexture(GL_TEXTURE_2D, hdrColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, _windowWidth, _windowHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColorBuffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "HDR FBO incomplete\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    std::default_random_engine generator;
    ssaoKernel.resize(64);
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / 64.0;
        scale = glm::mix(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel[i] = sample;
    }

    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            0.0f
        );
        ssaoNoise.push_back(noise);
    }
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 4, 4, 0, GL_RGB, GL_FLOAT, ssaoNoise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    _ssaoShader->use();
    for (unsigned int i = 0; i < 64; ++i) {
        _ssaoShader->setUniformVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
    }
    _ssaoShader->setUniformFloat("radius", ssaoRadius);
    _ssaoShader->setUniformFloat("bias", ssaoBias);
    _ssaoShader->setUniformMat4("projection", _camera.getProjectionMatrix());
    _ssaoShader->setUniformVec2("noiseScale", glm::vec2((float)_windowWidth / 4.0f, (float)_windowHeight / 4.0f));
}

void MazeApp::createMeshVAOs() {
    for (SceneModel& sm : _sceneModels) {
        if (!sm.model) continue;
        Model& model = *sm.model;
        const auto& verts = model.getVertices();
        const auto& inds = model.getIndices();

        // 若已存在任何一个 mesh.vao 则认为已初始化（避免重复）
        bool already = false;
        for (const Mesh& m : model.getMeshes()) if (m.vao != 0) { already = true; break; }
        if (already) continue;

        GLuint modelVao = 0, modelVbo = 0, modelEbo = 0;
        glGenVertexArrays(1, &modelVao);
        glGenBuffers(1, &modelVbo);
        glGenBuffers(1, &modelEbo);

        glBindVertexArray(modelVao);

        glBindBuffer(GL_ARRAY_BUFFER, modelVbo);
        glBufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)),
            verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(inds.size() * sizeof(uint32_t)),
            inds.data(), GL_STATIC_DRAW);

        const GLsizei stride = sizeof(Vertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, texCoord));

        glBindVertexArray(0);

        // 分配给每个 mesh（前提：mesh.indexOffset 已在 loadModelFromFile 中设置为对应 model 索引区间）
        for (Mesh& mesh : model.getMeshes()) {
            mesh.vao = modelVao;
            mesh.vbo = modelVbo;
            mesh.ebo = modelEbo;
        }
    }
}

void MazeApp::updateCamera(float deltaTime) {
    double xpos, ypos;
    glfwGetCursorPos(_window, &xpos, &ypos);

    float deltaX = static_cast<float>(xpos - _windowWidth / 2);
    float deltaY = static_cast<float>(_windowHeight / 2 - ypos);

    deltaX *= _mouseSensitivity;
    deltaY *= _mouseSensitivity;

    _yaw += -deltaX;
    _pitch += deltaY;

    if (_pitch > 89.0f) _pitch = 89.0f;
    if (_pitch < -89.0f) _pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
    front.y = sin(glm::radians(_pitch));
    front.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));
    front = glm::normalize(front);

    _camera.transform.rotation = glm::quatLookAt(front, Transform::getDefaultUp());

    glfwSetCursorPos(_window, _windowWidth / 2, _windowHeight / 2);

    glm::vec3 dir(0.0f);
    glm::vec3 horizontalFront = _camera.transform.getFront();
    horizontalFront.y = 0.0f;
    horizontalFront = glm::normalize(horizontalFront);

    if (_input.keyboard.keyStates[GLFW_KEY_W] == GLFW_PRESS) dir += horizontalFront;
    if (_input.keyboard.keyStates[GLFW_KEY_S] == GLFW_PRESS) dir -= horizontalFront;
    if (_input.keyboard.keyStates[GLFW_KEY_A] == GLFW_PRESS) dir -= _camera.transform.getRight();
    if (_input.keyboard.keyStates[GLFW_KEY_D] == GLFW_PRESS) dir += _camera.transform.getRight();
    if (_input.keyboard.keyStates[GLFW_KEY_Q] == GLFW_PRESS) dir += Transform::getDefaultUp();
    if (_input.keyboard.keyStates[GLFW_KEY_E] == GLFW_PRESS) dir -= Transform::getDefaultUp();

    if (glm::length(dir) > 0.0f) dir = glm::normalize(dir);

    glm::vec3 proposedPos = _camera.transform.position + dir * _moveSpeed * deltaTime;
    float playerRadius = 0.3f;  // 稍微增加碰撞半径，使碰撞检测更明显

    bool collided = false;
    for (const auto& sm : _sceneModels) {
        if (sm.isWall) {
            if (sm.aabb.intersects(proposedPos, playerRadius)) {
                collided = true;
                break;
            }
        }
    }

    // 如果没有碰撞，则应用移动
    if (!collided) {
        _camera.move(dir * _moveSpeed * deltaTime);
    }
}

void MazeApp::updateStars(float deltaTime) {
    for (auto& sm : _sceneModels) {
        if (sm.isStar) {
            sm.starTime += deltaTime;

            // 上下浮动：使用正弦波，范围更小，更贴近地面
            float floatOffset = 0.2f * sin(sm.starTime * 2.0f);  // 减小浮动幅度
            sm.transform.position.y = -1.5f + floatOffset;  // 基础高度降低，更贴近地面

            // 自旋：绕Y轴旋转
            float rotationAngle = sm.starTime * 1.5f;
            sm.transform.rotation = glm::angleAxis(rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }
}

void MazeApp::updateSunlight(float deltaTime) {
    if (!_sunAnimationEnabled) return;

    // 更新时间 (0-24小时循环)
    _sunTime += deltaTime * _sunAnimationSpeed;
    if (_sunTime >= 24.0f) _sunTime -= 24.0f;

    // 将时间转换为角度 (0-360度)
    float angle = (_sunTime / 24.0f) * 2.0f * glm::pi<float>();

    // 计算太阳方向 (从东升到西落的弧线)
    float elevation = glm::sin(angle);  // -1 到 1
    float azimuth = glm::cos(angle);

    // 设置光源位置 (模拟太阳轨迹)
    float radius = 50.0f;
    _lightPos = glm::vec3(
        azimuth * radius,
        glm::max(elevation * radius, -5.0f),  // 防止太阳到地平线以下太多
        glm::sin(angle * 0.3f) * radius * 0.3f  // 添加一些轨迹变化
    );

    // 根据时间计算太阳颜色和强度
    float normalizedTime = _sunTime / 24.0f;

    // 定义一天中的关键时刻颜色
    glm::vec3 nightColor(0.1f, 0.15f, 0.3f);      // 夜晚 - 深蓝
    glm::vec3 dawnColor(1.0f, 0.5f, 0.3f);        // 黎明 - 橙红
    glm::vec3 dayColor(1.0f, 0.95f, 0.9f);        // 白天 - 亮白
    glm::vec3 duskColor(1.0f, 0.4f, 0.2f);        // 黄昏 - 深橙

    // 根据时间段混合颜色
    glm::vec3 sunColor;
    float intensity;

    if (_sunTime < 6.0f) {
        // 深夜 (0-6点)
        float t = _sunTime / 6.0f;
        sunColor = nightColor;
        intensity = 0.1f + 0.1f * t;
    }
    else if (_sunTime < 8.0f) {
        // 黎明 (6-8点)
        float t = (_sunTime - 6.0f) / 2.0f;
        sunColor = glm::mix(nightColor, dawnColor, t);
        intensity = 0.2f + 0.5f * t;
    }
    else if (_sunTime < 10.0f) {
        // 早晨 (8-10点)
        float t = (_sunTime - 8.0f) / 2.0f;
        sunColor = glm::mix(dawnColor, dayColor, t);
        intensity = 0.7f + 0.3f * t;
    }
    else if (_sunTime < 16.0f) {
        // 白天 (10-16点)
        sunColor = dayColor;
        intensity = 1.0f;
    }
    else if (_sunTime < 18.0f) {
        // 傍晚 (16-18点)
        float t = (_sunTime - 16.0f) / 2.0f;
        sunColor = glm::mix(dayColor, duskColor, t);
        intensity = 1.0f - 0.3f * t;
    }
    else if (_sunTime < 20.0f) {
        // 黄昏 (18-20点)
        float t = (_sunTime - 18.0f) / 2.0f;
        sunColor = glm::mix(duskColor, nightColor, t);
        intensity = 0.7f - 0.5f * t;
    }
    else {
        // 夜晚 (20-24点)
        float t = (_sunTime - 20.0f) / 4.0f;
        sunColor = nightColor;
        intensity = 0.2f - 0.1f * t;
    }

    // 更新光源颜色和强度
    _lightColor = sunColor;
    _lightIntensity = intensity;

    // 根据时间调整环境光
    ambientStrength = 0.05f + 0.15f * intensity;
}

MazeApp::MazeApp(const Options& options)
    : Application(options), _camera(glm::radians(60.0f), static_cast<float>(options.windowWidth) / options.windowHeight, 0.1f, 100.0f) {
    glEnable(GL_DEPTH_TEST);
    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 设置初始位置在起点 'S' 附近的空地
    _camera.transform.position = glm::vec3(-20.0f, 0.0f, -12.0f);
    _camera.transform.lookAt(glm::vec3(0.0f, 0.0f, 0.0f));

    const char* vsCode =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(location = 2) in vec2 aTexCoord;\n"
        "uniform mat4 uModel;\n"
        "uniform mat4 uView;\n"
        "uniform mat4 uProj;\n"
        "out vec3 vNormal;\n"
        "out vec3 vWorldPos;\n"
        "out vec2 vTexCoord;\n"
        "void main() {\n"
        "    vec4 worldPos = uModel * vec4(aPos, 1.0);\n"
        "    vWorldPos = worldPos.xyz;\n"
        "    vNormal = mat3(transpose(inverse(uModel))) * aNormal;\n"
        "    vTexCoord = aTexCoord;\n"
        "    gl_Position = uProj * uView * worldPos;\n"
        "}\n";

    const char* fsCode =
        "#version 330 core\n"
        "in vec3 vNormal;\n"
        "in vec3 vWorldPos;\n"
        "in vec2 vTexCoord;\n"
        "uniform vec3 uLightDir;\n"
        "uniform vec3 uColor;\n"
        "uniform bool uHasTexture;\n"
        "uniform sampler2D uDiffuse;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    vec3 baseColor = uColor;\n"
        "    if (uHasTexture) {\n"
        "        baseColor = texture(uDiffuse, vTexCoord).rgb;\n"
        "    }\n"
        "    vec3 N = normalize(vNormal);\n"
        "    vec3 L = normalize(-uLightDir);\n"
        "    float diff = max(dot(N, L), 0.0);\n"
        "    vec3 ambient = 0.2 * baseColor;\n"
        "    vec3 diffuse = 0.8 * diff * baseColor;\n"
        "    FragColor = vec4(ambient + diffuse, 1.0);\n"
        "}\n";

    _shader.reset(new GLSLProgram());
    _shader->attachVertexShader(vsCode);
    _shader->attachFragmentShader(fsCode);
    _shader->link();

    initResources();
    createGBuffer();
    createSSAOBuffer();
    createMeshVAOs();

    try {
        const auto monsterModel = std::make_shared<Model>(
            loadModelFromFile(getAssetFullPath("obj/Monster.obj"), false));
        const auto judyModel = std::make_shared<Model>(
            loadModelFromFile(getAssetFullPath("obj/judy_3d.obj"), true));
        const auto nikeModel = std::make_shared<Model>(
            loadModelFromFile(getAssetFullPath("obj/nike.obj"), true));
        const auto snowModel = std::make_shared<Model>(
            loadModelFromFile(getAssetFullPath("obj/snow_box.obj"), true));
        const auto starModel = std::make_shared<Model>(
            loadModelFromFile(getAssetFullPath("obj/star.obj"), true));

        // 扩大迷宫：cellSize 从 1.5 增加到 2.5
        const float cellSize = 2.5f;
        const float wallY = -2.0f;
        const float groundY = -2.0f;  // 地面高度

        // 扩大的迷宫地图，增加出口
        const std::vector<std::string> maze = {
            "#####################",
            "S   #    #     #    #",
            "# ####   ###   #  # #",
            "#    #       # #  # #",
            "###  ####  ### ## # #",
            "#       #    #  # #  ",  // 右侧出口
            "# ####  #### ## # # #",
            "#    #       #    # #",
            "#    ### #####    # #",
            "#    #       #    # #",
            "# ####   ### #### # #",
            "#    #   #       ## #",
            "################### E",  // 底部出口（终点）
        };

        const int rows = static_cast<int>(maze.size());
        const int cols = static_cast<int>(maze[0].size());
        const float startX = -0.5f * cellSize * static_cast<float>(cols - 1);
        const float startZ = -0.5f * cellSize * static_cast<float>(rows - 1);

        // 辅助函数：将格子坐标转换为世界坐标
        const auto cellToWorld = [&](int c, int r, float y) -> glm::vec3 {
            return glm::vec3(
                startX + static_cast<float>(c) * cellSize,
                y,
                startZ + static_cast<float>(r) * cellSize);
            };

        // 创建墙壁 - 放大 snow_box 使其完全贴紧
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (maze[r][c] == '#') {
                    const glm::vec3 pos = cellToWorld(c, r, wallY);
                    SceneModel sm;
                    sm.model = snowModel;
                    sm.transform.position = pos;
                    sm.transform.scale = glm::vec3(2.8f);  // 进一步放大，确保完全闭合
                    sm.fallbackColor = glm::vec3(0.8f);
                    sm.isWall = true;

                    // 碰撞盒应该基于 cellSize
                    float halfCell = cellSize * 0.45f;
                    sm.aabb.min = glm::vec3(pos.x - halfCell, wallY - 1.0f, pos.z - halfCell);
                    sm.aabb.max = glm::vec3(pos.x + halfCell, wallY + 2.0f, pos.z + halfCell);

                    _sceneModels.push_back(std::move(sm));
                }
            }
        }

        // 在空地放置三个角色 - 确保它们在空地且站在地面上
        // 检查迷宫找几个确定的空地位置
        std::cout << "\n=== Character Placement ===" << std::endl;

        // Judy - 起点附近 (1, 2)
        {
            SceneModel judy;
            judy.model = judyModel;
            judy.transform.position = cellToWorld(2, 1, groundY);  // 直接放在地面上
            judy.transform.scale = glm::vec3(1.0f);
            judy.transform.lookAt(cellToWorld(4, 1, groundY));
            judy.fallbackColor = glm::vec3(0.7f, 0.7f, 0.9f);
            std::cout << "Judy at grid(1,2) -> world(" << judy.transform.position.x << ", "
                << judy.transform.position.y << ", " << judy.transform.position.z << ")" << std::endl;
            _sceneModels.push_back(std::move(judy));
        }

        // Nike - 在通道中 (1, 6)
        {
            SceneModel nike;
            nike.model = nikeModel;
            nike.transform.position = cellToWorld(6, 1, groundY);  // 直接放在地面上
            nike.transform.scale = glm::vec3(1.2f);
            nike.fallbackColor = glm::vec3(0.9f, 0.9f, 0.9f);
            std::cout << "Nike at grid(1,6) -> world(" << nike.transform.position.x << ", "
                << nike.transform.position.y << ", " << nike.transform.position.z << ")" << std::endl;
            _sceneModels.push_back(std::move(nike));
        }

        // Monster - 在另一个通道 (3, 2)
        {
            SceneModel monster;
            monster.model = monsterModel;
            monster.transform.position = cellToWorld(2, 3, groundY);  // 直接放在地面上
            monster.transform.scale = glm::vec3(1.5f);
            monster.fallbackColor = glm::vec3(0.8f, 0.7f, 0.6f);
            std::cout << "Monster at grid(3,2) -> world(" << monster.transform.position.x << ", "
                << monster.transform.position.y << ", " << monster.transform.position.z << ")" << std::endl;
            _sceneModels.push_back(std::move(monster));
        }

        // 放置更多星星在迷宫的空地上（避免与墙壁重合）
        // 只在空格' '的位置放置星星
        std::vector<std::pair<int, int>> starPositions = {
            // 起点区域的空地
            {1, 3}, {1, 4}, {1, 5},
            // 左上角空地
            {2, 7}, {3, 8}, {3, 3},{3,10},{3,11},{3,19},
            // 中间通道
            {4, 3}, {5, 5}, {6, 5},{5,10},{5,11},{5,19},
            // 右侧区域空地
            {1, 14}, {2, 16}, {3, 17},
            // 中下部空地
            {7, 3}, {7, 4}, {8, 3},{8,15},{8,16},{9,6},{9,7},{9,8},{9,14},{9,15},{9,19},
            // 底部空地
            {10, 6},{10,7}, { 10, 13 }, {11, 3},{11,4},{11,10}, { 11, 14 }
        };

        std::cout << "\n=== Star Positions ===" << std::endl;
        for (const auto& pos : starPositions) {
            // 验证位置是否为空地
            if (pos.first >= 0 && pos.first < rows &&
                pos.second >= 0 && pos.second < cols &&
                maze[pos.first][pos.second] == ' ') {

                SceneModel star;
                star.model = starModel;
                glm::vec3 worldPos = cellToWorld(pos.second, pos.first, groundY + 0.5f);  // 距离地面0.5单位
                star.transform.position = worldPos;
                star.transform.scale = glm::vec3(0.5f);  // 放大星星
                star.fallbackColor = glm::vec3(1.0f, 0.95f, 0.3f); // 亮黄色
                star.isStar = true;
                star.starTime = static_cast<float>(pos.first * 3 + pos.second * 2) * 0.3f;
                std::cout << "Star at grid(" << pos.first << "," << pos.second
                    << ") -> world(" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << ")" << std::endl;
                _sceneModels.push_back(std::move(star));
            }
            else {
                std::cout << "WARNING: Star position (" << pos.first << "," << pos.second
                    << ") is blocked or out of bounds!" << std::endl;
            }
        }
        std::cout << "Total stars placed: " << starPositions.size() << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        throw;
    }
}

MazeApp::~MazeApp() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void MazeApp::renderFrame() {
    float currentFrame = static_cast<float>(glfwGetTime());
    float deltaTime = currentFrame - _lastFrameTime;
    _lastFrameTime = currentFrame;

    updateCamera(deltaTime);
    updateStars(deltaTime);  // 更新星星动画
    updateSunlight(deltaTime);

    showFpsInWindowTitle();
    std::ostringstream title;
    title << "Zootopia gogogo | FPS: " << static_cast<int>(1.0f / deltaTime)
        << " | Light(" << std::fixed << std::setprecision(1)
        << _lightPos.x << "," << _lightPos.y << "," << _lightPos.z << ")"
        << " | Intensity:" << _lightIntensity
        << " | Exposure:" << exposure
        << " | SSAO:" << ssaoRadius
        << " | Ambient:" << ambientStrength
        << _lightPos.x << "," << _lightPos.y << "," << _lightPos.z << ")"
        << " | Sun Time: " << std::setprecision(1) << _sunTime << "h"
        << " | " << (_sunAnimationEnabled ? "ANIMATED" : "STATIC");
    glfwSetWindowTitle(_window, title.str().c_str());

    glClearColor(_clearColor.r, _clearColor.g, _clearColor.b, _clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::mat4 view = _camera.getViewMatrix();
    const glm::mat4 proj = _camera.getProjectionMatrix();

    _shader->use();
    _shader->setUniformMat4("uView", view);
    _shader->setUniformMat4("uProj", proj);
    _shader->setUniformInt("uDiffuse", 0);

    // 1. Geometry pass
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    _gBufferShader->use();
    glm::mat4 projection = _camera.getProjectionMatrix();

    _gBufferShader->setUniformMat4("view", view);
    _gBufferShader->setUniformMat4("projection", projection);

    for (const SceneModel& sm : _sceneModels) {
        if (!sm.model) continue;

        glm::mat4 model = sm.transform.getLocalMatrix();
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));

        _gBufferShader->setUniformMat4("model", model);
        _gBufferShader->setUniformMat3("normalMatrix", normalMat);

        for (const Mesh& mesh : sm.model->getMeshes()) {
            bool hasTexture = (mesh.diffuseTexture != nullptr);
            glm::vec3 finalColor = mesh.baseColor * sm.fallbackColor;

            _gBufferShader->setUniformVec3("fallbackColor", finalColor);
            _gBufferShader->setUniformBool("useAlbedoTexture", hasTexture);

            glActiveTexture(GL_TEXTURE0);
            if (hasTexture) {
                mesh.diffuseTexture->bind();
            }
            else {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glBindVertexArray(mesh.vao);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indexCount), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);

            if (hasTexture) mesh.diffuseTexture->unbind();
        }
    }

    _gBufferShader->unuse();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2. SSAO pass
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    _ssaoShader->use();
    _ssaoShader->setUniformMat4("projection", proj);
    _ssaoShader->setUniformFloat("radius", ssaoRadius);
    _ssaoShader->setUniformFloat("bias", ssaoBias);
    _ssaoShader->setUniformVec2("noiseScale",
    glm::vec2((float)_windowWidth / 4.0f, (float)_windowHeight / 4.0f));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    _ssaoShader->setUniformInt("gPosition", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    _ssaoShader->setUniformInt("gNormal", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    _ssaoShader->setUniformInt("texNoise", 2);

    glBindVertexArray(quadVAO);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 3. SSAO blur
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    _ssaoBlurShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    _ssaoBlurShader->setUniformInt("ssaoInput", 0);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 4. Lighting pass 
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    _lightingShader->use();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gPosition); _lightingShader->setUniformInt("gPosition", 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gNormal);   _lightingShader->setUniformInt("gNormal", 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gAlbedo);   _lightingShader->setUniformInt("gAlbedo", 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur); _lightingShader->setUniformInt("ssao", 3);

    _lightingShader->setUniformVec3("viewPos", _camera.transform.position);
    _lightingShader->setUniformVec3("lightPos", _lightPos);
    _lightingShader->setUniformVec3("lightColor", _lightColor * _lightIntensity);
    _lightingShader->setUniformFloat("ambientStrength", ambientStrength);
    _lightingShader->setUniformVec3("materialSpecular", _materialSpecular);
    _lightingShader->setUniformFloat("materialShininess", _materialShininess);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 5. HDR Tonemap + Gamma to default framebuffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    _hdrShader->use();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdrColorBuffer); _hdrShader->setUniformInt("hdrBuffer", 0);
    _hdrShader->setUniformFloat("exposure", exposure);
    _hdrShader->setUniformFloat("gamma", gammaVal);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void MazeApp::handleInput() {
    for (int i = 0; i <= GLFW_KEY_LAST; ++i) {
        _input.keyboard.keyStates[i] = glfwGetKey(_window, i);
    }

    if (_input.keyboard.keyStates[GLFW_KEY_ESCAPE] == GLFW_PRESS) {
        glfwSetWindowShouldClose(_window, true);
        return;
    }

    if (_windowReized) {
        _camera.aspect =
            static_cast<float>(_windowWidth) / static_cast<float>(_windowHeight);
        _windowReized = false;
    }

    float deltaTime = static_cast<float>(glfwGetTime()) - _lastFrameTime;

    if (_input.keyboard.keyStates[GLFW_KEY_KP_4] == GLFW_PRESS) {
        _lightPos.x -= _lightMoveSpeed * deltaTime;
    }
    if (_input.keyboard.keyStates[GLFW_KEY_KP_6] == GLFW_PRESS) {
        _lightPos.x += _lightMoveSpeed * deltaTime;
    }
    if (_input.keyboard.keyStates[GLFW_KEY_KP_8] == GLFW_PRESS) {
        _lightPos.z -= _lightMoveSpeed * deltaTime;
    }
    if (_input.keyboard.keyStates[GLFW_KEY_KP_2] == GLFW_PRESS) {
        _lightPos.z += _lightMoveSpeed * deltaTime;
    }
    if (_input.keyboard.keyStates[GLFW_KEY_KP_7] == GLFW_PRESS) {
        _lightPos.y += _lightMoveSpeed * deltaTime;
    }
    if (_input.keyboard.keyStates[GLFW_KEY_KP_9] == GLFW_PRESS) {
        _lightPos.y -= _lightMoveSpeed * deltaTime;
    }

    if (_input.keyboard.keyStates[GLFW_KEY_1] == GLFW_PRESS && !_keyPressed[GLFW_KEY_1]) {
        _lightIntensity = glm::max(0.1f, _lightIntensity - 0.1f);
        _keyPressed[GLFW_KEY_1] = true;
    }
    if (_input.keyboard.keyStates[GLFW_KEY_2] == GLFW_PRESS && !_keyPressed[GLFW_KEY_2]) {
        _lightIntensity += 0.1f;
        _keyPressed[GLFW_KEY_2] = true;
    }

    if (_input.keyboard.keyStates[GLFW_KEY_3] == GLFW_PRESS && !_keyPressed[GLFW_KEY_3]) {
        exposure = glm::max(0.1f, exposure - 0.1f);
        _keyPressed[GLFW_KEY_3] = true;
    }
    if (_input.keyboard.keyStates[GLFW_KEY_4] == GLFW_PRESS && !_keyPressed[GLFW_KEY_4]) {
        exposure += 0.1f;
        _keyPressed[GLFW_KEY_4] = true;
    }

    if (_input.keyboard.keyStates[GLFW_KEY_5] == GLFW_PRESS && !_keyPressed[GLFW_KEY_5]) {
        ssaoRadius = glm::max(0.1f, ssaoRadius - 0.05f);
        _keyPressed[GLFW_KEY_5] = true;
    }
    if (_input.keyboard.keyStates[GLFW_KEY_6] == GLFW_PRESS && !_keyPressed[GLFW_KEY_6]) {
        ssaoRadius += 0.05f;
        _keyPressed[GLFW_KEY_6] = true;
    }

    if (_input.keyboard.keyStates[GLFW_KEY_7] == GLFW_PRESS && !_keyPressed[GLFW_KEY_7]) {
        ambientStrength = glm::max(0.0f, ambientStrength - 0.05f);
        _keyPressed[GLFW_KEY_7] = true;
    }
    if (_input.keyboard.keyStates[GLFW_KEY_8] == GLFW_PRESS && !_keyPressed[GLFW_KEY_8]) {
        ambientStrength = glm::min(1.0f, ambientStrength + 0.05f);
        _keyPressed[GLFW_KEY_8] = true;
    }

    static bool key0WasPressed = false;
    bool key0Pressed = glfwGetKey(_window, GLFW_KEY_0) == GLFW_PRESS;
    if (key0Pressed && !key0WasPressed) {
        _sunAnimationEnabled = !_sunAnimationEnabled;
        std::cout << "Sun animation: " << (_sunAnimationEnabled ? "ON" : "OFF") << std::endl;
    }
    key0WasPressed = key0Pressed;

    for (int key : {GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4,
        GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8}) {
        if (_input.keyboard.keyStates[key] == GLFW_RELEASE) {
            _keyPressed[key] = false;
        }
    }
}