project(executable)

include(conan)

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

set(SOURCES 
    main.cpp)
add_executable(Chess ${SOURCES})
target_link_libraries(Chess 
    Vulkan::Vulkan 
    CONAN_PKG::glfw 
    CONAN_PKG::vkfw)

