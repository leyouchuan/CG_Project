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
    for (unsigned int i = 0; i < 64; ++i) {
        _ssaoShader->setUniformVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
    }
    _ssaoShader->setUniformFloat("radius", ssaoRadius);
    _ssaoShader->setUniformFloat("bias", ssaoBias);
    _ssaoShader->setUniformMat4("projection", _camera.getProjectionMatrix());
    _ssaoShader->setUniformVec2("noiseScale", glm::vec2((float)_windowWidth / 4.0f, (float)_windowHeight / 4.0f));
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
            float redPhase = sin(sm.starTime * 2.0f + sm.transform.position.x * 0.1f);
            float greenPhase = sin(sm.starTime * 2.3f + sm.transform.position.z * 0.1f + 1.0f);
            float bluePhase = sin(sm.starTime * 1.7f + sm.transform.position.x * 0.05f + sm.transform.position.z * 0.05f + 2.0f);
            
            sm.fallbackColor = glm::vec3(
                1.0f + 0.15f * redPhase,      // 红色分量动态变化
                0.95f + 0.2f * greenPhase,    // 绿色分量动态变化  
                0.3f + 0.25f * bluePhase      // 蓝色分量动态变化
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

        // 添加逼真的白雪地面 - 无限大的地面覆盖整个下方空间
        {
            // 创建一个巨大的地面，覆盖整个相机可见范围
            // 基于相机远裁剪面(200单位)和可能的移动范围
            float groundWidth = 1000.0f;  // 1000单位的宽度，确保完全覆盖
            float groundLength = 1000.0f; // 1000单位的长度
            
            // 主地面 - 中心区域
            SceneModel ground;
            ground.model = snowModel;  // 使用雪墙模型，这样会有雪纹理
            ground.transform.position = glm::vec3(0.0f, groundY - 0.1f, 0.0f);  // 略低于地面高度
            ground.transform.scale = glm::vec3(groundWidth / 2.0f, 0.05f, groundLength / 2.0f);  // 压扁成平面
            
            // 更逼真的雪颜色和材质特性
            // 真实的雪颜色：冷白色带轻微蓝色调，但不是纯蓝
            ground.fallbackColor = glm::vec3(0.96f, 0.96f, 1.01f);  // 逼真的雪白颜色
            
            // 轻微随机化颜色，创造更自然的效果
            float colorVariation = 0.02f;
            ground.fallbackColor += glm::vec3(
                (rand() % 100) / 1000.0f * colorVariation,
                (rand() % 100) / 1000.0f * colorVariation,
                (rand() % 100) / 1000.0f * colorVariation * 0.5f
            );
            
            // 注意：我们需要通过修改全局材质参数来影响地面的渲染效果
            // 雪应该有轻微的高光反射
            
            std::cout << "=== Realistic Snow Ground Added ===" << std::endl;
            std::cout << "Main ground size: " << groundWidth << " x " << groundLength << " units" << std::endl;
            std::cout << "Coverage: X[" << -groundWidth/2.0f << ", " << groundWidth/2.0f << "], " 
                      << "Z[" << -groundLength/2.0f << ", " << groundLength/2.0f << "]" << std::endl;
            std::cout << "Snow color: (" << ground.fallbackColor.r << ", " 
                      << ground.fallbackColor.g << ", " << ground.fallbackColor.b << ")" << std::endl;
            
            _sceneModels.push_back(std::move(ground));
            
            // 添加8个扩展地面，覆盖所有方向，形成几乎无限的地面
            std::vector<std::pair<float, float>> groundExtensions = {
                {groundWidth, 0.0f},           // 右
                {-groundWidth, 0.0f},          // 左
                {0.0f, groundLength},          // 前
                {0.0f, -groundLength},         // 后
                {groundWidth, groundLength},   // 右前
                {-groundWidth, groundLength},  // 左前
                {groundWidth, -groundLength},  // 右后
                {-groundWidth, -groundLength}  // 左后
            };
            
            std::cout << "=== Adding Realistic Snow Ground Extensions ===" << std::endl;
            for (const auto& ext : groundExtensions) {
                SceneModel groundExt;
                groundExt.model = snowModel;
                groundExt.transform.position = glm::vec3(ext.first, groundY - 0.1f, ext.second);
                groundExt.transform.scale = glm::vec3(groundWidth / 2.0f, 0.05f, groundLength / 2.0f);
                
                // 逼真的雪颜色，每块地面有轻微变化
                groundExt.fallbackColor = glm::vec3(0.96f, 0.96f, 1.01f);
                float colorVariation = 0.02f;
                groundExt.fallbackColor += glm::vec3(
                    (rand() % 100) / 1000.0f * colorVariation,
                    (rand() % 100) / 1000.0f * colorVariation,
                    (rand() % 100) / 1000.0f * colorVariation * 0.5f
                );
                
                _sceneModels.push_back(std::move(groundExt));
            }
            
            std::cout << "Total ground pieces: " << groundExtensions.size() + 1 << std::endl;
            std::cout << "Total coverage: " << groundWidth * 3.0f << " x " << groundLength * 3.0f << " units" << std::endl;
            std::cout << "Realistic snow ground with proper color and material effects!" << std::endl;
        }

        // ========== 添加冬季森林环境 ==========
        std::cout << "\n=== Adding Winter Forest Environment ===" << std::endl;
        
        // 加载雪松树模型
        const auto pineTreeModel = std::make_shared<Model>(
            loadModelFromFile(getAssetFullPath("obj/pine_tree.obj"), true));
        
        // 在迷宫周围随机放置雪松树
        const int treeCount = 25;  // 树的数量
        const float forestRadius = 35.0f;  // 森林半径
        const float minDistanceFromMaze = 15.0f;  // 离迷宫最近距离
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
        std::uniform_real_distribution<float> radiusDist(minDistanceFromMaze, minDistanceFromMaze + forestRadius);
        std::uniform_real_distribution<float> scaleDist(0.8f, 1.5f);  // 树的大小变化
        std::uniform_real_distribution<float> colorDist(0.95f, 1.05f);  // 颜色轻微变化
        
        int treesPlaced = 0;
        
        for (int i = 0; i < treeCount * 3; ++i) {  // 尝试更多次，确保放置足够树木
            // 随机极坐标
            float angle = angleDist(gen);
            float radius = radiusDist(gen);
            
            // 转换为笛卡尔坐标
            glm::vec3 treePos(
                radius * cos(angle),
                groundY - 0.1f,  // 与地面平齐
                radius * sin(angle)
            );
            
            // 检查是否太靠近迷宫中心（保留通道）
            if (glm::length(glm::vec2(treePos.x, treePos.z)) < minDistanceFromMaze) {
                continue;
            }
            
            // 创建雪松树
            SceneModel pineTree;
            pineTree.model = pineTreeModel;
            pineTree.transform.position = treePos;
            
            // 随机旋转和缩放
            float treeScale = scaleDist(gen);
            pineTree.transform.scale = glm::vec3(treeScale);
            pineTree.transform.rotation = glm::angleAxis(angleDist(gen), glm::vec3(0.0f, 1.0f, 0.0f));
            
            // 雪的覆盖程度变化
            float snowColorVariation = colorDist(gen);
            pineTree.fallbackColor = glm::vec3(
                0.96f * snowColorVariation,
                0.96f * snowColorVariation,
                1.01f * snowColorVariation
            );
            
            pineTree.isForestTree = true;
            
            _sceneModels.push_back(std::move(pineTree));
            treesPlaced++;
            
            if (treesPlaced >= treeCount) {
                break;
            }
        }
        
        std::cout << "Winter forest created with " << treesPlaced << " snow-covered pine trees!" << std::endl;
        std::cout << "Forest radius: " << forestRadius << " units" << std::endl;
        std::cout << "Trees placed around the maze at distance: " << minDistanceFromMaze << " to " 
                  << (minDistanceFromMaze + forestRadius) << " units" << std::endl;

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
    updateStars(deltaTime);

    showFpsInWindowTitle();
    std::ostringstream title;
    title << "Maze | FPS: " << static_cast<int>(1.0f / deltaTime)
        << " | Light(" << std::fixed << std::setprecision(1)
        << _lightPos.x << "," << _lightPos.y << "," << _lightPos.z << ")"
        << " | Intensity:" << _lightIntensity
        << " | Exposure:" << exposure
        << " | SSAO:" << ssaoRadius
        << " | Ambient:" << ambientStrength;
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
    _gBufferShader->setUniformFloat("time", static_cast<float>(glfwGetTime()));  // 传递时间用于星星发光特效

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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ========== 2. SSAO pass ==========
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

    // ========== 3. SSAO blur ==========
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    _ssaoBlurShader->use();
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    _ssaoBlurShader->setUniformInt("ssaoInput", 0);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ========== 4. Lighting pass ==========
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    _lightingShader->use();

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gPosition);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);

    _lightingShader->setUniformInt("gPosition", 0);
    _lightingShader->setUniformInt("gNormal", 1);
    _lightingShader->setUniformInt("gAlbedo", 2);
    _lightingShader->setUniformInt("ssao", 3);

    _lightingShader->setUniformVec3("viewPos", _camera.transform.position);
    _lightingShader->setUniformVec3("lightPos", _lightPos);
    _lightingShader->setUniformVec3("lightColor", _lightColor * _lightIntensity);
    _lightingShader->setUniformFloat("ambientStrength", ambientStrength);
    _lightingShader->setUniformVec3("materialSpecular", _materialSpecular);
    _lightingShader->setUniformFloat("materialShininess", _materialShininess);
    _lightingShader->setUniformFloat("time", static_cast<float>(glfwGetTime()));  // 传递时间用于星星动画

    glBindVertexArray(quadVAO);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ========== 5. 切换回默认帧缓冲 ==========
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(_clearColor.r, _clearColor.g, _clearColor.b, _clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 5.1 先画 HDR tonemap（物体）
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

    // 5.2 ★把几何体深度从 gBuffer 拷到默认帧缓冲（非常关键）
    int fbW, fbH;
    glfwGetFramebufferSize(_window, &fbW, &fbH);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer); // 这里用你的 gBuffer FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, fbW, fbH, 0, 0, fbW, fbH,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 5.3 最后画天空盒：只会出现在“没有几何体”的地方
    if (_skybox) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
        _skybox->draw(proj, viewNoTranslation);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }



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

    for (int key : {GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4,
        GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8}) {
        if (_input.keyboard.keyStates[key] == GLFW_RELEASE) {
            _keyPressed[key] = false;
        }
    }
}