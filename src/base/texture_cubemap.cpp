#include <cassert>
#include <stb_image.h>

#include "texture_cubemap.h"

TextureCubemap::TextureCubemap(
    GLint internalFormat, int width, int height, GLenum format, GLenum dataType) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, _handle);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);//绑定贴图、设定环绕和过滤方式

    for (uint32_t i = 0; i < 6; ++i) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, width, height, 0, format,
            dataType, nullptr);
    }
    //参数分别是：立方体的第i个面，mipmap级别0，内部格式，图像宽，图像高，边框0，原始像素数据的像素数组(data=stb_load()),都是需要传入的
    //这一步遍历了整个纹理目标
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);//解绑
}

TextureCubemap::TextureCubemap(TextureCubemap&& rhs) noexcept : Texture(std::move(rhs)) {}

void TextureCubemap::bind(int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, _handle);
}

void TextureCubemap::unbind() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void TextureCubemap::generateMipmap() const {
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void TextureCubemap::setParamterInt(GLenum name, int value) const {
    glTexParameteri(GL_TEXTURE_CUBE_MAP, name, value);
}

ImageTextureCubemap::ImageTextureCubemap(const std::vector<std::string>& filepaths)
    : _uris(filepaths) {
    assert(filepaths.size() == 6);
    // TODO: load six images and generate the texture cubemap
    // hint: you can refer to Texture2D(const std::string&) for image loading
    // write your code here
    // -----------------------------------------------
    // ...
    // -----------------------------------------------
    //将image加载到缓存
    glBindTexture(GL_TEXTURE_CUBE_MAP, _handle);
    //stbi_set_flip_vertically_on_load(true);
    for (size_t i = 0; i < 6; i++)
    {
        int width = 0, height = 0, channels = 0;
        unsigned char* data = stbi_load(filepaths[i].c_str(), &width, &height, &channels, 0);
        if (data == nullptr) {
            cleanup();
            throw std::runtime_error("load " + filepaths[i] + " failure");
        }

        GLenum format = GL_RGB;
        switch (channels) {
        case 1: format = GL_RED; break;
        case 3: format = GL_RGB; break;
        case 4: format = GL_RGBA; break;
        default:
            cleanup();
            stbi_image_free(data);
            throw std::runtime_error("unsupported format");
        }
        GLint alignment = 1;
        size_t pitch = static_cast<size_t>(width) * static_cast<size_t>(channels) * sizeof(unsigned char);
        if (pitch % 8 == 0) alignment = 8;
        else if (pitch % 4 == 0) alignment = 4;
        else if (pitch % 2 == 0) alignment = 2;
        else alignment = 1;
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

        // 把图像上传到 cubemap 的第 i 个面
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(i),
            0,
            static_cast<GLint>(format), // 可换为 GL_RGB8/GL_RGBA8 来明确 internal format
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            data);

        // 恢复默认对齐
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        // 释放 CPU 内存
        stbi_image_free(data);
    }

    // 设置默认参数
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    check();
}

ImageTextureCubemap::ImageTextureCubemap(ImageTextureCubemap&& rhs) noexcept
    : TextureCubemap(std::move(rhs)), _uris(std::move(rhs._uris)) {
    rhs._uris.clear();
}

const std::vector<std::string>& ImageTextureCubemap::getUris() const {
    return _uris;
}