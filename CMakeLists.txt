cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/src/cmake)

project(Chess LANGUAGES CXX)

if (CMAKE_EXPORT_COMPILE_COMMANDS)
    include(copy_compile_commands)
endif()

file(GLOB_RECURSE CONAN_RECIPES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/third_party_recipes/*/conanfile.py)
foreach(RECIPE ${CONAN_RECIPES})
    execute_process(COMMAND conan export ${RECIPE})
endforeach()

include_directories(${CMAKE_SOURCE_DIR}/src)

add_subdirectory(${CMAKE_SOURCE_DIR}/src/executable)

