project(utils CXX)

set(SOURCES 
    assert.cpp
    assert.hpp
    executable_folder.cpp
    executable_folder.hpp
)

add_library(utils ${SOURCES})
