project(executable)

include(conan)
include(add_glsl_shader)

conan_cmake_run(
    REQUIRES
    glfw/3.3.3
    vkfw/1.0.0
    OPTIONS
    vkfw:no_exceptions=True
    BASIC_SETUP CMAKE_TARGETS
    BUILD missing
)

find_package(Vulkan)

message(STATUS ${Vulkan_GLSLC_EXECUTABLE})

set(SOURCES 
    main.cpp
)
add_executable(Chess ${SOURCES})

add_glsl_shaders(Chess 
    shaders/shader.frag
    shaders/shader.vert
)

target_link_libraries(Chess 
    PUBLIC
    Vulkan::Vulkan 
    CONAN_PKG::glfw 
    CONAN_PKG::vkfw
)

