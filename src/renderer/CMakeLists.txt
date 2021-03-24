project(renderer CXX)

include(conan)
include(add_glsl_shader)

conan_cmake_run(
    REQUIRES
    glfw/3.3.3
    vkfw/1.0.0
    glm/0.9.9.8
    OPTIONS
    vkfw:no_exceptions=True
    BASIC_SETUP CMAKE_TARGETS
    BUILD missing
)

find_package(Vulkan)

set(SOURCES
    family_indeces.cpp
    family_indeces.hpp
    render_system.cpp
    render_system.hpp
)
add_library(renderer ${SOURCES})

add_glsl_shaders(renderer
    shaders/shader.frag
    shaders/shader.vert
)

target_compile_definitions(Vulkan::Vulkan
    INTERFACE
    VULKAN_HPP_NO_EXCEPTIONS
)

target_link_libraries(renderer
    PUBLIC
    utils
    Vulkan::Vulkan 
    CONAN_PKG::glfw 
    CONAN_PKG::vkfw
    CONAN_PKG::glm
)
