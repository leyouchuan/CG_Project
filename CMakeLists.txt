cmake_minimum_required(VERSION 3.20...4.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "utility")

# language settings
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_STANDARD 14)
#set(CMAKE_CXX_EXTENSIONS OFF)
set(SOURCE_PATH ${CMAKE_SOURCE_DIR}/src)
set(THIRD_PARTY_LIBRARY_PATH ${CMAKE_SOURCE_DIR}/external)
set(SHADER_TARGET_PATH ${CMAKE_SOURCE_DIR}/src/shaders)

project(final_project LANGUAGES C CXX VERSION 1.1)

#include("cmake/configure_project.cmake")
#include("cmake/hardlink_shaders.cmake")

# copy media data to the build directory
file(COPY "media/" DESTINATION "media")

add_subdirectory(${THIRD_PARTY_LIBRARY_PATH}/glfw)
add_subdirectory(${THIRD_PARTY_LIBRARY_PATH}/glad)
add_subdirectory(${THIRD_PARTY_LIBRARY_PATH}/imgui)
add_subdirectory(${THIRD_PARTY_LIBRARY_PATH}/stb)
add_subdirectory(${THIRD_PARTY_LIBRARY_PATH}/glm)
#add_subdirectory(${THIRD_PARTY_LIBRARY_PATH}/tinygltf)

file(GLOB BASE_HDR ${SOURCE_PATH}/base/*.h)
file(GLOB BASE_SRC ${SOURCE_PATH}/base/*.cpp)
file(GLOB PROJECT_HDR ${SOURCE_PATH}/*.h)
file(GLOB PROJECT_SRC ${SOURCE_PATH}/*.cpp)
file(GLOB PROJECT_SHADERS ${SOURCE_PATH}/shaders/*.vert ./*.geom ./*.frag)

add_executable(final_project ${BASE_HDR} ${BASE_SRC} ${PROJECT_HDR} ${PROJECT_SRC}  ${PROJECT_SHADERS})
source_group("Header Files" FILES ${BASE_HDR} ${PROJECT_HDR})
source_group("Source Files" FILES ${BASE_SRC} ${PROJECT_SRC})
source_group("Shader Files" FILES ${PROJECT_SHADERS})

if (win32)
set_target_properties(final_project PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Bin"
)
elsif(UNIX)
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        set_target_properties(final_project PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Bin/Debug"
        )
    else()
        set_target_properties(final_project PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Bin/Release"
        )
    endif()
endif()

target_include_directories(final_project PUBLIC
    ${THIRD_PARTY_LIBRARY_PATH}/glfw/include
    ${THIRD_PARTY_LIBRARY_PATH}/glad/include
    ${THIRD_PARTY_LIBRARY_PATH}/glm
    ${THIRD_PARTY_LIBRARY_PATH}/imgui
    ${THIRD_PARTY_LIBRARY_PATH}/stb
)


target_link_libraries(final_project PUBLIC glfw glad glm imgui stb)