#include "model.h"

#include "base/gl_utility.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <iostream>

Model::Model(const std::string& filepath) {
    std::cout << "Pretend loading model from: " << filepath << std::endl;

}
namespace {

    struct FaceVertex {
        int positionIndex = -1;
        int texcoordIndex = -1;
        int normalIndex = -1;
    };

    struct MaterialData {
        glm::vec3 diffuseColor = glm::vec3(0.8f);
        std::string diffuseTexture;
    };

    struct FaceGroup {
        std::string materialName;
        std::vector<FaceVertex> vertices;
    };

    struct ParsedObj {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texcoords;
        std::vector<FaceGroup> groups;
        std::unordered_map<std::string, MaterialData> materials;
    };

    glm::vec3 parseVec3(std::istringstream& iss) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        iss >> x >> y >> z;
        return { x, y, z };
    }

    glm::vec2 parseVec2(std::istringstream& iss) {
        float x = 0.0f, y = 0.0f;
        iss >> x >> y;
        return { x, y };
    }

    std::string trim(const std::string& s) {
        const auto begin = s.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return "";
        }
        const auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(begin, end - begin + 1);
    }

    std::vector<std::string> tokenize(const std::string& line) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }

    // 检查文件是否存在
    bool fileExists(const std::string& path) {
        std::ifstream f(path);
        return f.good();
    }

    // 从完整路径中提取基础名称（去除扩展名）
    std::string getBaseName(const std::string& path) {
        auto lastSlash = path.find_last_of("/\\");
        auto lastDot = path.find_last_of('.');

        size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
        size_t end = (lastDot == std::string::npos || lastDot < start) ? path.length() : lastDot;

        return path.substr(start, end - start);
    }

    void loadMtlFile(
        const std::string& mtlPath, std::unordered_map<std::string, MaterialData>& materials) {
        std::ifstream file(mtlPath);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open MTL file: " << mtlPath << std::endl;
            return;
        }

        std::cout << "Loading MTL file: " << mtlPath << std::endl;

        std::string line;
        std::string currentName;
        MaterialData currentMaterial;

        auto commitMaterial = [&]() {
            if (!currentName.empty()) {
                materials[currentName] = currentMaterial;
                std::cout << "  Material '" << currentName << "': ";
                std::cout << "Kd(" << currentMaterial.diffuseColor.r << ", "
                    << currentMaterial.diffuseColor.g << ", "
                    << currentMaterial.diffuseColor.b << ")";
                if (!currentMaterial.diffuseTexture.empty()) {
                    std::cout << ", Texture: '" << currentMaterial.diffuseTexture << "'";
                }
                std::cout << std::endl;
            }
            };

        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::istringstream iss(line);
            std::string key;
            iss >> key;

            if (key == "newmtl") {
                commitMaterial();
                iss >> currentName;
                currentMaterial = MaterialData{};
            }
            else if (key == "Kd") {
                currentMaterial.diffuseColor = parseVec3(iss);
            }
            else if (key == "map_Kd") {
                std::string texPath;
                std::getline(iss, texPath);
                texPath = trim(texPath);
                // 将反斜杠转换为正斜杠
                std::replace(texPath.begin(), texPath.end(), '\\', '/');
                currentMaterial.diffuseTexture = texPath;
            }
        }

        commitMaterial();
    }

    FaceVertex parseFaceToken(const std::string& token) {
        FaceVertex fv{};
        std::istringstream ss(token);
        std::string element;
        int idx = 0;
        while (std::getline(ss, element, '/')) {
            if (element.empty()) {
                ++idx;
                continue;
            }
            int value = std::stoi(element);
            int fixedIndex = (value < 0) ? value : value - 1;
            switch (idx) {
            case 0: fv.positionIndex = fixedIndex; break;
            case 1: fv.texcoordIndex = fixedIndex; break;
            case 2: fv.normalIndex = fixedIndex; break;
            default: break;
            }
            ++idx;
        }
        return fv;
    }

    ParsedObj parseObjFile(const std::string& path, bool loadMtl) {
        ParsedObj result;
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open obj file: " + path);
        }

        const auto lastSlash = path.find_last_of("/\\");
        const std::string directory = (lastSlash == std::string::npos) ? "" : path.substr(0, lastSlash + 1);

        std::string line;
        std::string currentMaterial;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::istringstream iss(line);
            std::string key;
            iss >> key;

            if (key == "v") {
                result.positions.push_back(parseVec3(iss));
            }
            else if (key == "vt") {
                result.texcoords.push_back(parseVec2(iss));
            }
            else if (key == "vn") {
                result.normals.push_back(parseVec3(iss));
            }
            else if (key == "f") {
                std::vector<std::string> tokens = tokenize(line);
                if (tokens.size() < 4) {
                    continue;
                }

                std::vector<FaceVertex> faceVertices;
                for (size_t i = 1; i < tokens.size(); ++i) {
                    faceVertices.push_back(parseFaceToken(tokens[i]));
                }

                for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                    FaceGroup group;
                    group.materialName = currentMaterial;
                    group.vertices.push_back(faceVertices[0]);
                    group.vertices.push_back(faceVertices[i]);
                    group.vertices.push_back(faceVertices[i + 1]);
                    result.groups.push_back(std::move(group));
                }
            }
            else if (key == "usemtl") {
                iss >> currentMaterial;
            }
            else if (key == "mtllib" && loadMtl) {
                std::string mtlFile;
                iss >> mtlFile;
                loadMtlFile(directory + mtlFile, result.materials);
            }
        }

        return result;
    }

    struct Hasher {
        size_t operator()(const Vertex& v) const {
            return std::hash<Vertex>{}(v);
        }
    };

} // namespace

Model::Model(std::vector<Mesh>&& meshes) : _meshes(std::move(meshes)) {}

Model::Model(Model&& rhs) noexcept : _meshes(std::move(rhs._meshes)) {
    rhs._meshes.clear();
}

Model& Model::operator=(Model&& rhs) noexcept {
    if (this != &rhs) {
        cleanup();
        _meshes = std::move(rhs._meshes);
        rhs._meshes.clear();
    }
    return *this;
}

Model::~Model() {
    cleanup();
}

void Model::draw() const {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Model::cleanup() {
    for (auto& mesh : _meshes) {
        if (mesh.ebo != 0) {
            glDeleteBuffers(1, &mesh.ebo);
            mesh.ebo = 0;
        }
        if (mesh.vbo != 0) {
            glDeleteBuffers(1, &mesh.vbo);
            mesh.vbo = 0;
        }
        if (mesh.vao != 0) {
            glDeleteVertexArrays(1, &mesh.vao);
            mesh.vao = 0;
        }
    }
}

const std::vector<Mesh>& Model::getMeshes() const {
    return _meshes;
}

Model loadModelFromFile(const std::string& path, bool loadMtl) {
    const ParsedObj parsed = parseObjFile(path, loadMtl);

    if (parsed.groups.empty()) {
        throw std::runtime_error("no faces found in obj: " + path);
    }

    glm::vec3 minV(std::numeric_limits<float>::max());
    glm::vec3 maxV(-std::numeric_limits<float>::max());

    for (const auto& group : parsed.groups) {
        for (const auto& fv : group.vertices) {
            int posIndex = fv.positionIndex;
            if (posIndex < 0) {
                posIndex = static_cast<int>(parsed.positions.size()) + posIndex;
            }
            if (posIndex < 0 || posIndex >= static_cast<int>(parsed.positions.size())) {
                continue;
            }
            const glm::vec3& p = parsed.positions[posIndex];
            minV = glm::min(minV, p);
            maxV = glm::max(maxV, p);
        }
    }

    const glm::vec3 center = 0.5f * (minV + maxV);
    const glm::vec3 extent = maxV - minV;
    const float maxExtent = std::max({ extent.x, extent.y, extent.z, 1e-4f });

    std::unordered_map<std::string, std::vector<FaceVertex>> groupedFaces;
    for (const auto& group : parsed.groups) {
        groupedFaces[group.materialName].insert(
            groupedFaces[group.materialName].end(), group.vertices.begin(), group.vertices.end());
    }

    std::vector<Mesh> meshes;
    std::unordered_map<std::string, std::shared_ptr<ImageTexture2D>> textureCache;

    const auto lastSlash = path.find_last_of("/\\");
    const std::string directory = (lastSlash == std::string::npos) ? "" : path.substr(0, lastSlash + 1);

    // 获取 OBJ 文件的基础名称（不含扩展名）
    const std::string baseName = getBaseName(path);
    std::cout << "Loading model: " << baseName << std::endl;

    for (const auto& groupPair : groupedFaces) {
        const std::string& materialName = groupPair.first;
        const auto& faceVertices = groupPair.second;

        std::unordered_map<Vertex, uint32_t, Hasher> uniqueVertices;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        vertices.reserve(faceVertices.size());
        indices.reserve(faceVertices.size());

        MaterialData material{};
        if (!materialName.empty()) {
            auto it = parsed.materials.find(materialName);
            if (it != parsed.materials.end()) {
                material = it->second;
            }
            else {
                std::cout << "Warning: Material '" << materialName << "' not found in MTL file" << std::endl;
            }
        }

        for (const auto& fv : faceVertices) {
            const auto resolveIndex = [](int idx, size_t size) {
                if (idx >= 0) {
                    return idx;
                }
                return static_cast<int>(size) + idx;
                };
            Vertex vertex{};
            const int posIndex = resolveIndex(fv.positionIndex, parsed.positions.size());
            if (posIndex >= 0 && posIndex < static_cast<int>(parsed.positions.size())) {
                vertex.position = (parsed.positions[posIndex] - center) / maxExtent;
            }
            const int normalIndex = resolveIndex(fv.normalIndex, parsed.normals.size());
            if (normalIndex >= 0 && normalIndex < static_cast<int>(parsed.normals.size())) {
                vertex.normal = parsed.normals[normalIndex];
            }
            const int texIndex = resolveIndex(fv.texcoordIndex, parsed.texcoords.size());
            if (texIndex >= 0 && texIndex < static_cast<int>(parsed.texcoords.size())) {
                vertex.texCoord = parsed.texcoords[texIndex];
            }

            auto it = uniqueVertices.find(vertex);
            if (it == uniqueVertices.end()) {
                uint32_t index = static_cast<uint32_t>(vertices.size());
                uniqueVertices[vertex] = index;
                vertices.push_back(vertex);
                indices.push_back(index);
            }
            else {
                indices.push_back(it->second);
            }
        }

        Mesh mesh{};
        mesh.baseColor = material.diffuseColor;

        // 纹理加载逻辑
        if (loadMtl) {
            std::string texturePath;

            // 策略1：如果 MTL 文件中指定了纹理，优先使用
            if (!material.diffuseTexture.empty()) {
                texturePath = directory + material.diffuseTexture;
                std::cout << "Trying texture from MTL: '" << texturePath << "'" << std::endl;
            }
            // 策略2：尝试查找与 OBJ 同名的纹理文件（支持多种扩展名）
            else {
                const std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".PNG", ".JPG", ".JPEG" };
                for (const auto& ext : extensions) {
                    std::string candidatePath = directory + baseName + ext;
                    if (fileExists(candidatePath)) {
                        texturePath = candidatePath;
                        std::cout << "Found auto-detected texture: '" << texturePath << "'" << std::endl;
                        break;
                    }
                }

                if (texturePath.empty()) {
                    std::cout << "No texture found for material '" << materialName << "'" << std::endl;
                }
            }

            // 加载纹理
            if (!texturePath.empty()) {
                auto cacheIt = textureCache.find(texturePath);
                if (cacheIt != textureCache.end()) {
                    mesh.diffuseTexture = cacheIt->second;
                    std::cout << "  -> Using cached texture" << std::endl;
                }
                else {
                    try {
                        auto texture = std::make_shared<ImageTexture2D>(texturePath);
                        textureCache[texturePath] = texture;
                        mesh.diffuseTexture = texture;
                        std::cout << "  -> Texture loaded successfully!" << std::endl;
                    }
                    catch (const std::exception& e) {
                        std::cerr << "  -> ERROR: Failed to load texture: " << e.what() << std::endl;
                        mesh.diffuseTexture = nullptr;
                    }
                }
            }
        }

        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glGenBuffers(1, &mesh.ebo);

        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(
            GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
            vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
            indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        mesh.indexCount = indices.size();
        meshes.push_back(std::move(mesh));
    }

    return Model(std::move(meshes));
}