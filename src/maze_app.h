#pragma once

#include "base/application.h"
#include "base/camera.h"
#include "base/glsl_program.h"
#include "base/transform.h"
#include "model.h"
#include <memory>
#include <vector>
#include <map>
#include <cmath>
#include <sstream>
#include <iomanip>

class MazeApp : public Application {
public:
    MazeApp(const Options& options);
    ~MazeApp();

private:
    struct AABB {
        glm::vec3 min;
        glm::vec3 max;

        bool intersects(const glm::vec3& point, float radius) const {
            glm::vec3 clamped = glm::clamp(point, min, max);
            return glm::length(clamped - point) < radius;
        }
    };

    struct SceneModel {
        std::shared_ptr<Model> model;
        Transform transform;
        glm::vec3 fallbackColor = glm::vec3(0.8f);
        AABB aabb;
        bool isWall = false;
        bool isStar = false;  // 新增：标记是否为星星
        float starTime = 0.0f;  // 新增：星星动画时间
    };

    PerspectiveCamera _camera;
    std::unique_ptr<GLSLProgram> _shader;
    std::vector<SceneModel> _sceneModels;

    float _yaw = -90.0f;
    float _pitch = 0.0f;
    float _moveSpeed = 2.0f;
    float _mouseSensitivity = 0.02f;

    virtual void handleInput();
    virtual void renderFrame();

    void renderUI();

    //fbos

    //fbos
    void createGBuffer();
    void createSSAOBuffer();
    void updateCamera(float deltaTime);
    void updateStars(float deltaTime);  // 新增：更新星星动画

    float _lastFrameTime = 0.0f;

    struct GBufferUniforms {
        GLint model = -1;
        GLint view = -1;
        GLint projection = -1;
        GLint normalMatrix = -1;
        GLint fallbackColor = -1;
        GLint useAlbedoTexture = -1;
        GLint albedoTex = -1;
    } _gBufferUniforms;

    std::unique_ptr<GLSLProgram> _gBufferShader;
    std::unique_ptr<GLSLProgram> _ssaoShader;
    std::unique_ptr<GLSLProgram> _ssaoBlurShader;
    std::unique_ptr<GLSLProgram> _lightingShader;
    std::unique_ptr<GLSLProgram> _hdrShader;

    GLuint gBuffer = 0;
    GLuint gPosition = 0, gNormal = 0, gAlbedo = 0;
    GLuint rboDepth = 0;

    GLuint ssaoFBO = 0, ssaoBlurFBO = 0;
    GLuint ssaoColorBuffer = 0, ssaoColorBufferBlur = 0;

    GLuint hdrFBO = 0;
    GLuint hdrColorBuffer = 0;

    GLuint quadVAO = 0, quadVBO = 0;

    std::vector<glm::vec3> ssaoKernel;
    GLuint noiseTexture = 0;

    void initResources();

    float ssaoRadius = 0.5f;
    float ssaoBias = 0.025f;
    float ambientStrength = 0.12f;
    float exposure = 1.0f;
    float gammaVal = 2.2f;
    glm::vec3 _lightPos = glm::vec3(0.0f, 4.0f, 0.0f);
    glm::vec3 _lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 _materialSpecular = glm::vec3(0.5f);
    float _materialShininess = 32.0f;

    float _lightIntensity = 1.0f;

    const float _lightMoveSpeed = 5.0f;
    const float _paramAdjustSpeed = 0.5f;

    // --- Sun / time of day 控制 ---
    bool _sunAuto = true;              // 自动运行时间
    float _timeOfDay = 12.0f;          // 当前时间（小时，0..24），初始中午
    float _timeScale = 60.0f;          // 模拟速度：真实秒 -> 模拟分钟（例如 60 => 1s = 1min）
    float _lastSunUpdate = 0.0f;       // 用于增量计算
    glm::vec3 _sunDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 _sunColor = glm::vec3(1.0f, 1.0f, 0.95f);
    float _sunIntensity = 1.0f;
};

    // --- Sun / time of day 控制 ---
    bool _sunAuto = true;              // 自动运行时间
    float _timeOfDay = 12.0f;          // 当前时间（小时，0..24），初始中午
    float _timeScale = 60.0f;          // 模拟速度：真实秒 -> 模拟分钟（例如 60 => 1s = 1min）
    float _lastSunUpdate = 0.0f;       // 用于增量计算
    glm::vec3 _sunDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 _sunColor = glm::vec3(1.0f, 1.0f, 0.95f);
    float _sunIntensity = 1.0f;
};
