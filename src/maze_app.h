#pragma once

#include "base/application.h"
#include "base/camera.h"
#include "base/glsl_program.h"
#include "base/transform.h"
#include "model.h"
#include "base/skybox.h"
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
            // 找到 AABB 上距离点最近的点
            glm::vec3 closest;
            closest.x = glm::clamp(point.x, min.x, max.x);
            closest.y = glm::clamp(point.y, min.y, max.y);
            closest.z = glm::clamp(point.z, min.z, max.z);

            // 计算距离
            float distanceSquared = glm::dot(point - closest, point - closest);
            return distanceSquared < (radius * radius);
        }
    };

    struct SceneModel {
        std::shared_ptr<Model> model;
        Transform transform;
        glm::vec3 fallbackColor = glm::vec3(0.8f);
        AABB aabb;
        bool isWall = false;
        bool isStar = false;  // 标记是否为星星
        bool isForestTree = false;  // 新增：标记是否为森林树木
        float starTime = 0.0f;  // 星星动画时间
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

    void createGBuffer();
    void createSSAOBuffer();
    void createMeshVAOs();
    void updateCamera(float deltaTime);
    void updateStars(float deltaTime);

    void updateSunlight(float deltaTime);

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
    float ambientStrength = 0.15f;  // 增加环境光强度，模拟雪地的反射
    float exposure = 1.0f;
    float gammaVal = 2.2f;
    glm::vec3 _lightPos = glm::vec3(5.0f, 8.0f, 5.0f);  // 冬季斜阳位置
    glm::vec3 _lightColor = glm::vec3(0.95f, 0.98f, 1.05f);  // 冷白色调，略带蓝色
    glm::vec3 _materialSpecular = glm::vec3(0.6f, 0.6f, 0.7f);  // 雪的镜面反射（偏蓝）
    float _materialShininess = 40.0f;  // 雪的微光泽

    float _lightIntensity = 1.0f;

    const float _lightMoveSpeed = 5.0f;
    const float _paramAdjustSpeed = 0.5f;

    std::map<int, bool> _keyPressed;


    float _sunTime = 0.0f;          // 太阳时间 (0-24小时循环)
    bool _sunAnimationEnabled = false;  // 是否启用太阳动画
    float _sunAnimationSpeed = 1.0f;    // 动画速度倍率

    // 添加天空盒
    std::unique_ptr<SkyBox> _skybox;

};