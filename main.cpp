#include "maze_app.h"
#include <iostream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <string>

std::string getExecutableDir() {
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return std::string();
    std::string path(buf, buf + len);
    auto pos = path.find_last_of("\\/");
    if (pos != std::string::npos) path.resize(pos);
    return path;
}
#else
#include <unistd.h>
#include <limits.h>
#include <string>

std::string getExecutableDir() {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return std::string();
    std::string path(buf, buf + len);
    auto pos = path.find_last_of('/');
    if (pos != std::string::npos) path.resize(pos);
    return path;
}
#endif

Options getOptions(int argc, char* argv[]) {
    Options options;
    options.windowTitle = "Zootopia gogogo";
    options.windowWidth = 1280;
    options.windowHeight = 720;
    options.windowResizable = true;
    options.vSync = true;
    options.msaa = true;
    options.glVersion = {3, 3};
    options.backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    std::string exeDir = getExecutableDir();
    if (!exeDir.empty())
        options.assetRootDir = exeDir + "/media/";
    else
        options.assetRootDir = "media/";

    return options;
}

int main(int argc, char* argv[]) {
    Options options = getOptions(argc, argv);

    try {
        MazeApp app(options);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        exit(EXIT_FAILURE);
    }

    return 0;
}
