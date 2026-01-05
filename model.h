#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include "base/transform.h"
#include "base/texture2d.h"
#include "base/vertex.h"

struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    size_t indexCount = 0;
    glm::vec3 baseColor = glm::vec3(1.0f);
    std::shared_ptr<ImageTexture2D> diffuseTexture;
};


class Model {
public:
    Model(const std::string& filepath);
    
   

    Model() = default;

    explicit Model(std::vector<Mesh>&& meshes);

    Model(const Model&) = delete;

    Model& operator=(const Model&) = delete;

    Model(Model&& rhs) noexcept;

    Model& operator=(Model&& rhs) noexcept;

    ~Model();

    virtual void draw() const;

    const std::vector<Mesh>& getMeshes() const;

public:
    Transform transform;

protected:
    // vertices of the table represented in model's own coordinate
    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;

    // bounding box
    //BoundingBox _boundingBox;

    // opengl objects
    GLuint _vao = 0;
    GLuint _vbo = 0;
    GLuint _ebo = 0;

    GLuint _boxVao = 0;
    GLuint _boxVbo = 0;
    GLuint _boxEbo = 0;

    /*void computeBoundingBox();

    void initGLResources();

    void initBoxGLResources();*/

    void cleanup();


private:
    std::vector<Mesh> _meshes;

};

Model loadModelFromFile(const std::string& path, bool loadMtl);
