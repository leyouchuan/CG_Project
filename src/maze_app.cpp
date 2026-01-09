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
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cstring>



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


        // 冬季森林天空盒 - 使用现有的天空盒，适合冬季环境
        // 注意：天空盒纹理顺序通常是：right, left, top, bottom, front, back
        std::vector<std::string> skyboxTextures = {
            getAssetFullPath("texture/skybox/Right_Tex.jpg"),    // +X (right)
            getAssetFullPath("texture/skybox/Left_Tex.jpg"),     // -X (left)
            getAssetFullPath("texture/skybox/Up_Tex.jpg"),       // +Y (top) - 冬季灰白色天空
            getAssetFullPath("texture/skybox/Down_Tex.jpg"),     // -Y (bottom) - 雪地
            getAssetFullPath("texture/skybox/Front_Tex.jpg"),    // +Z (front)
            getAssetFullPath("texture/skybox/Back_Tex.jpg")      // -Z (back)
        };

        // 检查纹理文件是否存在，如果不存在则使用备用路径
        bool allFilesExist = true;
        for (const auto& path : skyboxTextures) {
            std::ifstream file(path);
            if (!file.good()) {
                std::cerr << "Skybox texture not found: " << path << std::endl;
                allFilesExist = false;
                break;
            }
        }

        if (allFilesExist) {
            _skybox = std::make_unique<SkyBox>(skyboxTextures);
            std::cerr << "Skybox loaded successfully!" << std::endl;
        }
        else {
            // 使用简单天空盒或跳过
            std::cerr << "Using fallback skybox colors" << std::endl;
            // 可以创建一个纯色天空盒或使用程序生成的
        }
    }
    catch (const std::exception& e) {
        std::cerr << "initResources failed: " << e.what() << std::endl;
        throw;
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
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
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

    // 获取当前激活的OpenGL程序ID
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

    GLint samplesLoc = glGetUniformLocation(currentProgram, "samples");

    if (samplesLoc != -1) {
        // 转换为float数组
        std::vector<float> samplesData;
        samplesData.reserve(64 * 3);

        for (int i = 0; i < 64; ++i) {
            samplesData.push_back(ssaoKernel[i].x);
            samplesData.push_back(ssaoKernel[i].y);
            samplesData.push_back(ssaoKernel[i].z);
        }

        // 一次性设置整个数组
        glUniform3fv(samplesLoc, 64, samplesData.data());
        std::cerr << "SSAO samples set successfully" << std::endl;
    }
    else {
        std::cerr << "ERROR: 'samples' uniform not found" << std::endl;
    }
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
    // ★★★ 添加这个if ★★★
    if (_mouseControlEnabled) {
        double xpos, ypos;
        glfwGetCursorPos(_window, &xpos, &ypos);

        float deltaX = static_cast<float>(xpos - _windowWidth / 2);
        float deltaY = static_cast<float>(_windowHeight / 2 - ypos);

        deltaX *= _mouseSensitivity;
        deltaY *= _mouseSensitivity;

        _yaw += deltaX;
        _pitch += deltaY;

        if (_pitch > 89.0f) _pitch = 89.0f;
        if (_pitch < -89.0f) _pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
        front.y = sin(glm::radians(_pitch));
        front.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));
        front = glm::normalize(front);

        _camera.transform.rotation = glm::quatLookAt(front, Transform::getDefaultUp());

        glfwSetCursorPos(_window, _windowWidth / 2, _windowHeight / 2);  // ← 这行也要在if里
    }
    // ★★★ if结束，鼠标代码到此为止 ★★★

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

        // ★★★ 新的脚印逻辑：在移动后调用 ★★★
        _footprintSystem.updateFootprints(_camera.transform.position);
    }

    // 更新脚印系统（衰减等）
    updateFootprints(deltaTime);
}

void MazeApp::updateFootprints(float deltaTime) {
    _footprintSystem.update(deltaTime);
}


void MazeApp::updateStars(float deltaTime) {
    for (auto& sm : _sceneModels) {
        if (sm.isStar) {
            sm.starTime += deltaTime;

            // 复杂的三维浮动：多个正弦波叠加
            float floatOffsetX = 0.05f * sin(sm.starTime * 1.5f + sm.transform.position.x);
            float floatOffsetZ = 0.05f * cos(sm.starTime * 1.8f + sm.transform.position.z);
            float floatOffsetY = 0.2f * sin(sm.starTime * 3.0f);  // 主上下浮动

            sm.transform.position.x += floatOffsetX * deltaTime * 0.5f;
            sm.transform.position.z += floatOffsetZ * deltaTime * 0.5f;
            sm.transform.position.y = -1.5f + floatOffsetY;  // 基础高度降低，更贴近地面

            // 复杂的旋转：绕多个轴旋转，形成有趣的动态效果
            float rotationYAngle = sm.starTime * 2.5f;  // Y轴旋转
            float rotationXAngle = 0.2f * sin(sm.starTime * 1.5f);  // X轴摆动
            float rotationZAngle = 0.1f * cos(sm.starTime * 2.0f);  // Z轴摆动

            glm::quat rotationY = glm::angleAxis(rotationYAngle, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat rotationX = glm::angleAxis(rotationXAngle, glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat rotationZ = glm::angleAxis(rotationZAngle, glm::vec3(0.0f, 0.0f, 1.0f));

            sm.transform.rotation = rotationY * rotationX * rotationZ;

            // 增强的bulingbuling发光特效：多重频率叠加
            float glowIntensity1 = 0.8f + 0.4f * sin(sm.starTime * 5.0f);  // 快速闪烁
            float glowIntensity2 = 0.9f + 0.3f * sin(sm.starTime * 8.0f + 1.0f);  // 更快速闪烁
            float glowIntensity3 = 0.7f + 0.5f * sin(sm.starTime * 2.0f + 2.0f);  // 慢速闪烁

            float combinedGlow = (glowIntensity1 * 0.4f + glowIntensity2 * 0.3f + glowIntensity3 * 0.3f);

            // 丰富的星光颜色变化：彩虹般的渐变效果
            float goldPhase = sin(sm.starTime * 2.5f + sm.transform.position.x * 0.1f);
            float redPhase = 0.9f + 0.1f * goldPhase;
            float greenPhase = 0.7f + 0.2f * goldPhase;
            float bluePhase = 0.1f + 0.1f * sin(sm.starTime * 4.0f); // 微量蓝波动

            sm.fallbackColor = glm::vec3(
                redPhase,      // 红：基准0.9 + 小波动
                greenPhase,    // 绿：基准0.7 + 小波动
                bluePhase      // 蓝：基准0.1 + 快速闪烁
            ) * combinedGlow;

            // 动态缩放脉冲效果：多个频率叠加
            float pulseScale1 = 0.5f + 0.08f * sin(sm.starTime * 3.5f);
            float pulseScale2 = 0.03f * sin(sm.starTime * 7.0f + 0.5f);
            float pulseScale3 = 0.02f * cos(sm.starTime * 4.0f + 1.5f);

            sm.transform.scale = glm::vec3(pulseScale1 + pulseScale2 + pulseScale3);

            // 添加轻微的颜色溢出效果：让星星的颜色影响周围环境
            sm.fallbackColor += glm::vec3(
                0.05f * sin(sm.starTime * 4.0f),
                0.04f * cos(sm.starTime * 5.0f),
                0.03f * sin(sm.starTime * 3.0f)
            );
        }
    }
}

void MazeApp::updateSunlight(float deltaTime) {
    if (!_sunAnimationEnabled) return;

    //时间变化
    _sunTime += deltaTime * _sunAnimationSpeed;
    if (_sunTime >= 24.0f) _sunTime -= 24.0f;

    float angle = (_sunTime / 24.0f) * 2.0f * glm::pi<float>();

    // 计算太阳方向
    float elevation = glm::sin(angle);  // -1 到 1
    float azimuth = glm::cos(angle);

    // 设置光源位置
    float radius = 50.0f;
    _lightPos = glm::vec3(
        azimuth * radius,
        glm::max(elevation * radius, -5.0f),
        glm::sin(angle * 0.3f) * radius * 0.3f
    );

    // 根据时间计算太阳颜色和强度
    float normalizedTime = _sunTime / 24.0f;

    // 特定时刻颜色
    glm::vec3 nightColor(0.1f, 0.15f, 0.3f);
    glm::vec3 dawnColor(1.0f, 0.5f, 0.3f);
    glm::vec3 dayColor(1.0f, 0.95f, 0.9f);
    glm::vec3 duskColor(1.0f, 0.4f, 0.2f);

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
        float t = (_sunTime - 18.0f) / 2.0f;
        sunColor = glm::mix(duskColor, nightColor, t);
        intensity = 0.7f - 0.5f * t;
    }
    else {
        float t = (_sunTime - 20.0f) / 4.0f;
        sunColor = nightColor;
        intensity = 0.2f - 0.1f * t;
    }

    _lightColor = sunColor;
    _lightIntensity = intensity;

    ambientStrength = 0.05f + 0.15f * intensity;
}

void MazeApp::initFootprintSystem() {
    _footprintSystem.init(_windowWidth, _windowHeight);

    // 创建脚印渲染的四边形
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &_footprintVAO);
    glGenBuffers(1, &_footprintVBO);

    glBindVertexArray(_footprintVAO);
    glBindBuffer(GL_ARRAY_BUFFER, _footprintVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}



MazeApp::MazeApp(const Options& options)
    : Application(options), _camera(glm::radians(60.0f), static_cast<float>(options.windowWidth) / options.windowHeight, 0.1f, 200.0f) {
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

        // 创建墙壁 - 确保墙底和雪地底部平齐
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (maze[r][c] == '#') {
                    const glm::vec3 pos = cellToWorld(c, r, groundY - 0.2); // 直接使用groundY作为墙底基准
                    SceneModel sm;
                    sm.model = snowModel;

                    // 关键：调整scale的Y轴，让模型底部贴合groundY（假设snowModel的中心在0,0,0）
                    // 原scale Y轴是2.8f，改为让模型高度从groundY开始向上延伸
                    sm.transform.position = pos + glm::vec3(0.0f, 1.4f, 0.0f); // 模型中心上移1.4f（2.8f/2）
                    sm.transform.scale = glm::vec3(2.8f, 2.8f, 2.8f);         // 保持等比缩放
                    sm.fallbackColor = glm::vec3(0.8f);
                    sm.isWall = true;

                    // 碰撞盒同步调整（确保碰撞检测和视觉一致）
                    float halfCell = cellSize * 0.45f;
                    sm.aabb.min = glm::vec3(pos.x - halfCell, groundY, pos.z - halfCell); // 碰撞盒底部=groundY
                    sm.aabb.max = glm::vec3(pos.x + halfCell, groundY + 2.8f, pos.z + halfCell); // 碰撞盒高度=2.8f

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
            judy.transform.position = cellToWorld(2, 1, groundY + 0.5);  // 直接放在地面上
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
                star.fallbackColor = glm::vec3(1.0f, 0.8f, 0.2f); // 初始金橙色
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

        // 添加逼真的白雪地面 - 无限大的地面覆盖整个下方空间
        {
            float groundWidth = 1000.0f;
            float groundLength = 1000.0f;

            float snowThickness = 0.05f;                     // 雪地厚度
            float snowCenterY = groundY + snowThickness * 0.5f; // ✅ 关键：保证雪地底部=groundY

            // 主地面 - 中心区域
            SceneModel ground;
            ground.model = snowModel;
            ground.transform.position = glm::vec3(0.0f, snowCenterY, 0.0f);   // ✅ 用 snowCenterY
            ground.transform.scale = glm::vec3(groundWidth / 2.0f, snowThickness, groundLength / 2.0f);

            ground.fallbackColor = glm::vec3(0.96f, 0.96f, 1.01f);
            float colorVariation = 0.02f;
            ground.fallbackColor += glm::vec3(
                (rand() % 100) / 1000.0f * colorVariation,
                (rand() % 100) / 1000.0f * colorVariation,
                (rand() % 100) / 1000.0f * colorVariation * 0.5f
            );

            _sceneModels.push_back(std::move(ground));

            // 扩展地面
            std::vector<std::pair<float, float>> groundExtensions = {
                {groundWidth, 0.0f}, {-groundWidth, 0.0f},
                {0.0f, groundLength}, {0.0f, -groundLength},
                {groundWidth, groundLength}, {-groundWidth, groundLength},
                {groundWidth, -groundLength}, {-groundWidth, -groundLength}
            };

            for (const auto& ext : groundExtensions) {
                SceneModel groundExt;
                groundExt.model = snowModel;
                groundExt.transform.position = glm::vec3(ext.first, snowCenterY, ext.second);  // ✅ 同样用 snowCenterY
                groundExt.transform.scale = glm::vec3(groundWidth / 2.0f, snowThickness, groundLength / 2.0f);

                groundExt.fallbackColor = glm::vec3(0.96f, 0.96f, 1.01f);
                float colorVariation2 = 0.02f;
                groundExt.fallbackColor += glm::vec3(
                    (rand() % 100) / 1000.0f * colorVariation2,
                    (rand() % 100) / 1000.0f * colorVariation2,
                    (rand() % 100) / 1000.0f * colorVariation2 * 0.5f
                );

                _sceneModels.push_back(std::move(groundExt));
            }
        }


        // ========== 添加冬季森林环境 ==========
        std::cout << "\n=== Adding Winter Forest Environment ===" << std::endl;

        // 加载雪松树模型
        const auto pineTreeModel = std::make_shared<Model>(
            loadModelFromFile(getAssetFullPath("obj/pine_tree.obj"), true));

        // 迷宫核心参数（用于边界判断）
        //const float cellSize = 2.5f;
        //const int rows = static_cast<int>(maze.size());
        //const int cols = static_cast<int>(maze[0].size());
        const float mazeHalfWidth = 0.5f * cellSize * static_cast<float>(cols - 1);
        const float mazeHalfLength = 0.5f * cellSize * static_cast<float>(rows - 1);
        const float mazeBoundaryExpand = 5.0f; // 迷宫边界外扩5单位，确保树木远离迷宫

        // 雪地高度系统（统一基准）
        //const float groundY = -2.0f;                  // 雪地底部高度
        const float snowThickness = 0.05f;            // 雪地厚度
        const float snowSurfaceY = groundY + snowThickness;  // 雪地上表面高度（核心对齐基准）

        // 树木生成参数
        const int treeCount = 25;                     // 目标树木数量
        const float minDistanceFromMaze = mazeBoundaryExpand;  // 离迷宫边界最小距离
        const float maxDistanceFromMaze = 40.0f;      // 离迷宫边界最大距离
        const float treeBurialDepth = 0.5f;           // 树木埋入雪地深度（设为0=刚好贴合）
        const float treeBaseHeight = 2.0f;            // 松树模型原始高度（需根据实际obj调整，假设中心在0，底部-1.0，顶部+1.0）

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * glm::pi<float>());
        std::uniform_real_distribution<float> radiusDist(minDistanceFromMaze, maxDistanceFromMaze);
        std::uniform_real_distribution<float> scaleDist(3.0f, 5.0f);  // 树木缩放比例
        std::uniform_real_distribution<float> colorDist(0.95f, 1.05f);  // 颜色轻微变化
        std::uniform_real_distribution<float> rotationDist(0.0f, 2.0f * glm::pi<float>()); // 随机旋转

        int treesPlaced = 0;

        // 循环直到放置足够数量的树木
        while (treesPlaced < treeCount) {
            // 1. 随机生成极坐标（保证在迷宫外围）
            float angle = angleDist(gen);
            float radius = radiusDist(gen);

            // 转换为笛卡尔坐标（X/Z平面）
            float treeX = radius * cos(angle);
            float treeZ = radius * sin(angle);

            // 2. 严格判断：是否在迷宫区域内（包含外扩边界）
            bool isInMazeArea = false;
            if (abs(treeX) <= mazeHalfWidth + mazeBoundaryExpand &&
                abs(treeZ) <= mazeHalfLength + mazeBoundaryExpand) {
                isInMazeArea = true;
            }
            if (isInMazeArea) {
                continue; // 跳过迷宫区域内的位置
            }

            // 3. 计算树木Y轴位置 - 直接放在地面
            float treeScale = scaleDist(gen);
            float treeHalfHeight = (treeBaseHeight / 2.0f) * treeScale;
            float treePosY = groundY + treeHalfHeight - 2.5f;  // ← 修改这行

            // 4. 构建最终树木位置
            glm::vec3 treePos(treeX, treePosY, treeZ);

            // 5. 创建雪松树实例
            SceneModel pineTree;
            pineTree.model = pineTreeModel;
            pineTree.transform.position = treePos;
            pineTree.transform.scale = glm::vec3(treeScale);
            pineTree.transform.rotation = glm::angleAxis(rotationDist(gen), glm::vec3(0.0f, 1.0f, 0.0f)); // 随机Y轴旋转

            // 6. 雪色变化（保留自然感）
            float snowColorVar = colorDist(gen);
            pineTree.fallbackColor = glm::vec3(
                0.96f * snowColorVar,
                0.96f * snowColorVar,
                1.01f * snowColorVar
            );

            // 7. 添加到场景并计数
            _sceneModels.push_back(std::move(pineTree));
            treesPlaced++;
            std::cout << "Placed tree " << treesPlaced << " at (" << treeX << ", " << treePosY << ", " << treeZ << ")" << std::endl;
        }

        std::cout << "Successfully placed " << treesPlaced << " pine trees outside maze!" << std::endl;
    }


    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        throw;
    }

    // ======================== 新增：ImGui初始化 ========================
    // 1. 检查ImGui版本，创建上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // 启用键盘导航（可选，方便UI操作）
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // 设置深色主题（可选，视觉更适配雪地场景）
    ImGui::StyleColorsDark();
    // 2. 绑定GLFW窗口（_window是你的GLFW窗口句柄，已在Application中创建）
    ImGui_ImplGlfw_InitForOpenGL(_window, true);
    // 3. 初始化OpenGL3后端，匹配GLSL版本（330）
    ImGui_ImplOpenGL3_Init("#version 330");
    // ==================================================================

    // 在这里初始化脚印系统，确保所有资源都已加载
    initFootprintSystem();
    std::cout << "Footprint system initialized. FBO: " << _footprintSystem.getTexture() << std::endl;
}

MazeApp::~MazeApp() {
    // ======================== 优化：ImGui清理（带判空保护） ========================
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    // ======================== 新增：OpenGL资源清理（避免内存泄漏） ========================
    // 清理GBuffer相关
    glDeleteFramebuffers(1, &gBuffer);
    glDeleteTextures(1, &gPosition);
    glDeleteTextures(1, &gNormal);
    glDeleteTextures(1, &gAlbedo);
    glDeleteRenderbuffers(1, &rboDepth);

    // 清理SSAO相关
    glDeleteFramebuffers(1, &ssaoFBO);
    glDeleteFramebuffers(1, &ssaoBlurFBO);
    glDeleteTextures(1, &ssaoColorBuffer);
    glDeleteTextures(1, &ssaoColorBufferBlur);
    glDeleteTextures(1, &noiseTexture);

    // 清理HDR相关
    glDeleteFramebuffers(1, &hdrFBO);
    glDeleteTextures(1, &hdrColorBuffer);

    // 清理脚印相关VAO/VBO
    glDeleteVertexArrays(1, &_footprintVAO);
    glDeleteBuffers(1, &_footprintVBO);

    // 清理后处理四边形VAO/VBO
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);

    // 清理自定义shader
    if (_shader) {
        _shader.reset(); // 释放GLSLProgram（内部应调用glDeleteProgram）
    }

    // 清理GBuffer/SSAO/Lighting/HDR shader（如果有独立声明）
    if (_gBufferShader) _gBufferShader.reset();
    if (_ssaoShader) _ssaoShader.reset();
    if (_ssaoBlurShader) _ssaoBlurShader.reset();
    if (_lightingShader) _lightingShader.reset();
    if (_hdrShader) _hdrShader.reset();

    // 清理天空盒（如果SkyBox类有资源需要释放）
    if (_skybox) {
        _skybox.reset();
    }
}

void MazeApp::renderFrame() {

    float currentFrame = static_cast<float>(glfwGetTime());
    float deltaTime = currentFrame - _lastFrameTime;
    _lastFrameTime = currentFrame;

    updateCamera(deltaTime);
    updateStars(deltaTime);
    updateSunlight(deltaTime);

    // ===== ImGui 核心帧循环（必须在绘制窗口前执行）=====
    // 初始化ImGui帧（如果全局ImGui已初始化，这里是每帧必做）
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame(); // 关键：标记ImGui进入帧范围，解决g=nullptr问题

    // 3. 绘制脚印编辑器窗口（F键切换_showFootprintEditor）
    if (_showFootprintEditor) {
        _footprintSystem.renderUI(_showFootprintEditor);

    }

    

    // ========== 修复：窗口标题重复拼接问题 ==========
    showFpsInWindowTitle();
    std::ostringstream title;
    title << "Zootopia gogogo | FPS: " << static_cast<int>(1.0f / deltaTime)
        << " | Light(" << std::fixed << std::setprecision(1)
        << _lightPos.x << "," << _lightPos.y << "," << _lightPos.z << ")"
        << " | Intensity:" << _lightIntensity
        << " | Exposure:" << exposure
        << " | SSAO:" << ssaoRadius
        << " | Ambient:" << ambientStrength
        << " | Sun Time: " << std::setprecision(1) << _sunTime << "h"
        << " | " << (_sunAnimationEnabled ? "ANIMATED" : "STATIC");
    glfwSetWindowTitle(_window, title.str().c_str());

    const glm::mat4 view = _camera.getViewMatrix();
    const glm::mat4 proj = _camera.getProjectionMatrix();

    // ========== 1. Geometry pass ==========
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    _gBufferShader->use();
    _gBufferShader->setUniformMat4("view", view);
    _gBufferShader->setUniformMat4("projection", proj);
    _gBufferShader->setUniformFloat("time", static_cast<float>(glfwGetTime()));

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

    // ========== 2. 脚印效果渲染到纹理 ==========
    _footprintSystem.renderFootprintsToTexture();

    // ========== 3. SSAO pass ==========
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    _ssaoShader->use();
    _ssaoShader->setUniformMat4("projection", proj);
    _ssaoShader->setUniformFloat("radius", ssaoRadius);
    _ssaoShader->setUniformFloat("bias", ssaoBias);
    _ssaoShader->setUniformVec2("noiseScale",
        glm::vec2((float)_windowWidth / 4.0f, (float)_windowHeight / 4.0f));

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gPosition);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, noiseTexture);

    _ssaoShader->setUniformInt("gPosition", 0);
    _ssaoShader->setUniformInt("gNormal", 1);
    _ssaoShader->setUniformInt("texNoise", 2);

    glBindVertexArray(quadVAO);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ========== 4. SSAO blur ==========
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    _ssaoBlurShader->use();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    _ssaoBlurShader->setUniformInt("ssaoInput", 0);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ========== 5. Lighting pass ==========
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    _lightingShader->use();



    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gPosition);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);

    // 添加脚印纹理
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, _footprintSystem.getTexture());

    // 设置所有uniform - 添加缺失的脚印相关uniform
    _lightingShader->setUniformInt("gPosition", 0);
    _lightingShader->setUniformInt("gNormal", 1);
    _lightingShader->setUniformInt("gAlbedo", 2);
    _lightingShader->setUniformInt("ssao", 3);
    _lightingShader->setUniformInt("footprintMap", 4); // 添加脚印纹理uniform


    // 添加脚印参数
    _lightingShader->setUniformVec3("footprintColor", _footprintSystem.params.color);
    _lightingShader->setUniformFloat("footprintRadius", _footprintSystem.params.radius);
    _lightingShader->setUniformFloat("footprintRoughness", _footprintSystem.params.roughness);

    // ... 其他uniform设置 ...
    _lightingShader->setUniformVec3("viewPos", _camera.transform.position);
    _lightingShader->setUniformVec3("lightPos", _lightPos);
    _lightingShader->setUniformVec3("lightColor", _lightColor * _lightIntensity);
    _lightingShader->setUniformFloat("ambientStrength", ambientStrength);
    _lightingShader->setUniformVec3("materialSpecular", _materialSpecular);
    //_lightingShader->setUniformFloat("materialShininess", _materialShininess);
    _lightingShader->setUniformFloat("time", static_cast<float>(glfwGetTime()));  // 传递时间用于星星动画

    

    glBindVertexArray(quadVAO);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ========== 6. 切换回默认帧缓冲 ==========
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(_clearColor.r, _clearColor.g, _clearColor.b, _clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // =========HDR===================
    _hdrShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrColorBuffer);
    _hdrShader->setUniformInt("hdrBuffer", 0);
    _hdrShader->setUniformFloat("exposure", exposure);
    _hdrShader->setUniformFloat("gamma", gammaVal);

    glBindVertexArray(quadVAO);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_BLEND); // 如果后面要画 ImGui

    // 6.2 ★把几何体深度从 gBuffer 拷到默认帧缓冲（非常关键）
    int fbW, fbH;
    glfwGetFramebufferSize(_window, &fbW, &fbH);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer); // 这里用你的 gBuffer FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, fbW, fbH, 0, 0, fbW, fbH,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 6.3 最后画天空盒：只会出现在“没有几何体”的地方
    if (_skybox) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
        _skybox->draw(proj, viewNoTranslation);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    // ===== 关键修正：ImGui绘制前恢复GL默认状态 =====
    glEnable(GL_BLEND); // ImGui需要混合模式显示半透明UI
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // ImGui默认混合方式
    glDisable(GL_DEPTH_TEST); // ImGui不需要深度测试（避免被遮挡）
    glDisable(GL_CULL_FACE); // 禁用面剔除（确保ImGui元素都显示）

    // ===== ImGui 最终渲染 =====
    ImGui::Render(); // 生成ImGui绘制数据
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // 绘制到屏幕

    // ===== 恢复GL状态（可选，避免影响下一帧）=====
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // ===== Screenshot (F12) =====
// Note: this captures the default framebuffer's BACK buffer before swap.
    if (_screenshotRequested) {
        saveScreenshotToFile();
        _screenshotRequested = false;
    }

    // ===== 注意：交换缓冲区已经在 Application::run() 中调用 =====
    // 这里不需要再次调用 glfwSwapBuffers(_window);
    // 双重交换缓冲区会导致闪烁问题

}



void MazeApp::handleInput() {
    for (int i = GLFW_KEY_SPACE; i <= GLFW_KEY_LAST; ++i) {
        _input.keyboard.keyStates[i] = glfwGetKey(_window, i);
    }

    if (_input.keyboard.keyStates[GLFW_KEY_ESCAPE] == GLFW_PRESS) {
        glfwSetWindowShouldClose(_window, true);
        return;
    }

    // ★★★ TAB键切换（绝对安全版本）★★★
    static bool tabKeyWasPressed = false;
    bool tabKeyPressed = glfwGetKey(_window, GLFW_KEY_TAB) == GLFW_PRESS;

    if (tabKeyPressed && !tabKeyWasPressed) {
        // 只要有ImGui窗口打开，就完全忽略TAB
        if (!_showFootprintEditor) {
            _mouseControlEnabled = !_mouseControlEnabled;

            if (_mouseControlEnabled) {
                glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                std::cout << "Mouse: ON (camera control)" << std::endl;
            }
            else {
                glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "Mouse: OFF (cursor visible)" << std::endl;
            }
        }
        // 有窗口时完全不输出任何信息，完全忽略TAB
    }
    tabKeyWasPressed = tabKeyPressed;


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

    // P: screenshot (edge-triggered)
    bool shotPressed = (glfwGetKey(_window, GLFW_KEY_P) == GLFW_PRESS);
    static bool wasPressed = false;
    if (shotPressed && !wasPressed) {
        _screenshotRequested = true;
    }
    wasPressed = shotPressed;

    // F: 打开/关闭脚印编辑器
    static bool fKeyWasPressed = false;
    bool fKeyPressed = glfwGetKey(_window, GLFW_KEY_F) == GLFW_PRESS;
    if (fKeyPressed && !fKeyWasPressed) {
        // ★ 保护：如果ImGui有活动控件，拒绝操作
        if (!ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
            _showFootprintEditor = !_showFootprintEditor;

            if (_showFootprintEditor) {
                _mouseControlEnabled = false;
                glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "UI opened - Cursor visible" << std::endl;
            }
            else {
                _mouseControlEnabled = true;
                glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                std::cout << "UI closed - Game mode" << std::endl;
            }
        }
        else {
            std::cout << "Cannot close - Release UI control first" << std::endl;
        }
    }
    fKeyWasPressed = fKeyPressed;


   




    for (int key : {GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4,
        GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8}) {
        if (_input.keyboard.keyStates[key] == GLFW_RELEASE) {
            _keyPressed[key] = false;
        }
    }


}

// ===== Screenshot implementation =====

bool MazeApp::writeBMP(const std::string& filepath,
    int width,
    int height,
    const std::vector<unsigned char>& rgbBottomUp) {
    if (width <= 0 || height <= 0) return false;
    if ((int)rgbBottomUp.size() < width * height * 3) return false;

    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    const int rowStride = width * 3;
    const int padding = (4 - (rowStride % 4)) % 4;
    const int pixelDataSize = (rowStride + padding) * height;
    const int fileSize = 14 + 40 + pixelDataSize;

    auto writeU16 = [&](uint16_t v) {
        unsigned char b[2] = { (unsigned char)(v & 0xFF), (unsigned char)((v >> 8) & 0xFF) };
        out.write(reinterpret_cast<char*>(b), 2);
        };
    auto writeU32 = [&](uint32_t v) {
        unsigned char b[4] = {
            (unsigned char)(v & 0xFF),
            (unsigned char)((v >> 8) & 0xFF),
            (unsigned char)((v >> 16) & 0xFF),
            (unsigned char)((v >> 24) & 0xFF)
        };
        out.write(reinterpret_cast<char*>(b), 4);
        };
    auto writeS32 = [&](int32_t v) {
        writeU32(static_cast<uint32_t>(v));
        };

    // BITMAPFILEHEADER (14 bytes)
    out.put('B');
    out.put('M');
    writeU32((uint32_t)fileSize);
    writeU16(0);
    writeU16(0);
    writeU32(14 + 40); // offset

    // BITMAPINFOHEADER (40 bytes)
    writeU32(40);
    writeS32(width);
    writeS32(height); // positive => bottom-up (matches glReadPixels)
    writeU16(1);      // planes
    writeU16(24);     // bpp
    writeU32(0);      // BI_RGB
    writeU32((uint32_t)pixelDataSize);
    writeS32(2835);   // 72 DPI
    writeS32(2835);
    writeU32(0);
    writeU32(0);

    // Pixel data: BGR + row padding
    std::vector<unsigned char> row;
    row.resize((size_t)rowStride);
    const unsigned char pad[3] = { 0, 0, 0 };

    for (int y = 0; y < height; ++y) {
        const unsigned char* src = rgbBottomUp.data() + (size_t)y * (size_t)rowStride;
        for (int x = 0; x < width; ++x) {
            const unsigned char r = src[x * 3 + 0];
            const unsigned char g = src[x * 3 + 1];
            const unsigned char b = src[x * 3 + 2];
            row[x * 3 + 0] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
        out.write(reinterpret_cast<const char*>(row.data()), rowStride);
        if (padding) out.write(reinterpret_cast<const char*>(pad), padding);
    }

    return out.good();
}

void MazeApp::saveScreenshotToFile() {
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(_window, &fbW, &fbH);
    if (fbW <= 0 || fbH <= 0) return;

    std::vector<unsigned char> pixels;
    pixels.resize((size_t)fbW * (size_t)fbH * 3);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, fbW, fbH, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // screenshots/<timestamp>_<counter>.bmp
    std::filesystem::path outDir = std::filesystem::current_path() / "screenshots";
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream name;
    name << "screenshot_" << std::put_time(&tm, "%Y%m%d_%H%M%S")
        << "_" << std::setw(4) << std::setfill('0') << (_screenshotCounter++)
        << ".bmp";

    std::filesystem::path outPath = outDir / name.str();

    if (writeBMP(outPath.string(), fbW, fbH, pixels)) {
        std::cout << "Saved screenshot: " << outPath.string() << std::endl;
    }
    else {
        std::cerr << "Failed to save screenshot: " << outPath.string() << std::endl;
    }
}

// ==================== FootprintSystem 实现 ====================

void FootprintSystem::FootprintParams::renderEditorUI() {
    ImGui::ColorEdit3("Footprint Color", &color.x);
    ImGui::SliderFloat("Radius", &radius, 0.01f, 1.0f, "%.3f");
    ImGui::SliderFloat("Depth", &depth, 0.05f, 1.0f, "%.2f");
    ImGui::SliderFloat("Roughness", &roughness, 0.1f, 1.0f);
    ImGui::SliderFloat("Decay Time", &decayTime, 5.0f, 120.0f, "%.1f s");
    ImGui::SliderFloat("Intensity", &fadeSpeed, 0.1f, 2.0f, "%.2f");
}

void FootprintSystem::init(int width, int height) {
    // 创建脚印渲染纹理
    glGenFramebuffers(1, &_footprintFBO);
    glGenTextures(1, &_footprintTexture);

    glBindTexture(GL_TEXTURE_2D, _footprintTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindFramebuffer(GL_FRAMEBUFFER, _footprintFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _footprintTexture, 0);

    // 创建ping-pong FBO用于模糊
    for (int i = 0; i < 2; i++) {
        glGenFramebuffers(1, &_pingpongFBO[i]);
        glGenTextures(1, &_pingpongTexture[i]);

        glBindTexture(GL_TEXTURE_2D, _pingpongTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glBindFramebuffer(GL_FRAMEBUFFER, _pingpongFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _pingpongTexture[i], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 创建脚印着色器
    const char* footprintVs = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    const char* footprintFs = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        
        uniform vec3 footprintPositions[100];
        uniform float footprintIntensities[100];
        uniform int footprintCount;
        uniform vec3 footprintColor;
        uniform float footprintRadius;
        uniform float footprintDepth;
        uniform vec2 resolution;
        
        void main() {
            vec2 uv = TexCoord;
            vec3 color = vec3(0.0);
            float totalWeight = 0.0;
            
            for (int i = 0; i < footprintCount; i++) {
                // 转换为纹理空间坐标
                vec2 footprintUV = vec2(
                    (footprintPositions[i].x + 50.0) / 100.0,
                    (footprintPositions[i].z + 50.0) / 100.0
                );
                
                float dist = distance(uv, footprintUV);
                if (dist < footprintRadius) {
                    float weight = footprintIntensities[i] * (1.0 - dist / footprintRadius);
                    color += footprintColor * weight;
                    totalWeight += weight;
                }
            }
            
            if (totalWeight > 0.0) {
                color /= totalWeight;
                FragColor = vec4(color, min(totalWeight * footprintDepth, 1.0));
            } else {
                FragColor = vec4(0.0);
            }
        }
    )";

    _footprintShader = std::make_unique<GLSLProgram>();
    _footprintShader->attachVertexShader(footprintVs);
    _footprintShader->attachFragmentShader(footprintFs);
    _footprintShader->link();

    // 创建模糊着色器
    const char* blurFs = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        
        uniform sampler2D image;
        uniform bool horizontal;
        uniform float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
        
        void main() {
            vec2 tex_offset = 1.0 / textureSize(image, 0);
            vec3 result = texture(image, TexCoord).rgb * weight[0];
            
            if (horizontal) {
                for (int i = 1; i < 5; ++i) {
                    result += texture(image, TexCoord + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                    result += texture(image, TexCoord - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                }
            } else {
                for (int i = 1; i < 5; ++i) {
                    result += texture(image, TexCoord + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                    result += texture(image, TexCoord - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                }
            }
            
            FragColor = vec4(result, texture(image, TexCoord).a);
        }
    )";

    _blurShader = std::make_unique<GLSLProgram>();
    _blurShader->attachVertexShader(footprintVs);
    _blurShader->attachFragmentShader(blurFs);
    _blurShader->link();
}

void FootprintSystem::updateFootprints(const glm::vec3& cameraPosition) {
    glm::vec3 currentPos = cameraPosition;

    // 检查是否在地面附近（地面上方一定高度内）
    bool isNearGround = (currentPos.y >= _groundY) &&
        (currentPos.y <= _groundY + _groundTolerance);

    if (isNearGround) {
        if (!_hasLastFootprint) {
            // 第一个脚印
            glm::vec3 footprintPos = currentPos;
            footprintPos.y = _groundY;  // 固定在地面高度

            addFootprint(footprintPos);
            _lastFootprintPos = footprintPos;
            _hasLastFootprint = true;

            std::cout << "First footprint at: ("
                << footprintPos.x << ", "
                << footprintPos.y << ", "
                << footprintPos.z << ")" << std::endl;
        }
        else {
            // ★ 关键：计算XZ平面的水平移动距离（忽略Y轴）
            float horizontalDist = glm::length(glm::vec2(
                currentPos.x - _lastFootprintPos.x,
                currentPos.z - _lastFootprintPos.z
            ));

            // 移动了足够距离才添加新脚印
            if (horizontalDist >= _footprintStepDistance) {
                glm::vec3 footprintPos = currentPos;
                footprintPos.y = _groundY;

                addFootprint(footprintPos);
                _lastFootprintPos = footprintPos;

                std::cout << "Footprint added at: ("
                    << footprintPos.x << ", "
                    << footprintPos.y << ", "
                    << footprintPos.z << ") dist="
                    << horizontalDist << std::endl;
            }
        }
    }
    else {
        // 离开地面区域，重置状态
        _hasLastFootprint = false;
    }
}

void FootprintSystem::update(float deltaTime) {
    // 移除过期的脚印
    float currentTime = static_cast<float>(glfwGetTime());
    _footprints.erase(
        std::remove_if(_footprints.begin(), _footprints.end(),
            [currentTime, this](const auto& fp) {
                return currentTime - std::get<1>(fp) > params.decayTime;
            }),
        _footprints.end()
    );
}

void FootprintSystem::addFootprint(const glm::vec3& position) {
    _footprints.push_back(std::make_tuple(position, static_cast<float>(glfwGetTime())));

    // 限制最大脚印数量
    if (_footprints.size() > 100) {
        _footprints.erase(_footprints.begin());
    }
}

void FootprintSystem::renderFootprintsToTexture() {
    if (_footprints.empty()) return;

    // 创建渲染用的VAO（如果还没有）
    static GLuint renderVAO = 0;
    static GLuint renderVBO = 0;
    if (renderVAO == 0) {
        float quadVertices[] = {
            -1.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 0.0f,

            -1.0f,  1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f
        };

        glGenVertexArrays(1, &renderVAO);
        glGenBuffers(1, &renderVBO);

        glBindVertexArray(renderVAO);
        glBindBuffer(GL_ARRAY_BUFFER, renderVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _footprintFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    _footprintShader->use();

    // 传递脚印数据
    int count = static_cast<int>(std::min(_footprints.size(), size_t(100)));
    float currentTime = static_cast<float>(glfwGetTime());

    for (int i = 0; i < count; i++) {
        glm::vec3 pos = std::get<0>(_footprints[i]);
        float createTime = std::get<1>(_footprints[i]);
        float age = currentTime - createTime;
        float intensity = glm::clamp(1.0f - (age / params.decayTime), 0.0f, 1.0f);

        _footprintShader->setUniformVec3("footprintPositions[" + std::to_string(i) + "]", pos);
        _footprintShader->setUniformFloat("footprintIntensities[" + std::to_string(i) + "]",
            intensity * params.fadeSpeed);
    }

    _footprintShader->setUniformInt("footprintCount", count);
    _footprintShader->setUniformVec3("footprintColor", params.color);
    _footprintShader->setUniformFloat("footprintRadius", params.radius);
    _footprintShader->setUniformFloat("footprintDepth", params.depth);
    //_footprintShader->setUniformVec2("resolution", glm::vec2(1024, 1024));

    // 绘制
    glBindVertexArray(renderVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // 应用模糊
    applyBlur();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



void FootprintSystem::applyBlur() {
    bool horizontal = true;
    bool first_iteration = true;

    // 创建blur用的VAO（如果还没有）
    static GLuint blurVAO = 0;
    static GLuint blurVBO = 0;
    if (blurVAO == 0) {
        float quadVertices[] = {
            -1.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 0.0f,

            -1.0f,  1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f
        };

        glGenVertexArrays(1, &blurVAO);
        glGenBuffers(1, &blurVBO);

        glBindVertexArray(blurVAO);
        glBindBuffer(GL_ARRAY_BUFFER, blurVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, _pingpongFBO[horizontal]);
        _blurShader->use();
        _blurShader->setUniformBool("horizontal", horizontal);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? _footprintTexture : _pingpongTexture[!horizontal]);

        glBindVertexArray(blurVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }

    // 最后结果存回_footprintTexture
    glBindFramebuffer(GL_FRAMEBUFFER, _footprintFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    _blurShader->use();
    _blurShader->setUniformBool("horizontal", false);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _pingpongTexture[!horizontal]);

    glBindVertexArray(blurVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void FootprintSystem::renderUI(bool& showEditor) {
    if (!ImGui::Begin("Footprint Editor", &showEditor)) {
        ImGui::End();  // 关键：即使窗口折叠也要调用 End()
        return;
    }

    params.renderEditorUI();

    ImGui::Separator();
    ImGui::Text("Footprint Count: %zu", _footprints.size());

    if (ImGui::Button("Clear All Footprints")) {
        _footprints.clear();
    }



    ImGui::End();
}


