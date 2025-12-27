cmake_minimum_required(VERSION 3.15)
project(crow_project)

set(CMAKE_CXX_STANDARD 17)

add_executable(crow_app main.cpp)

# Link Windows socket library
if(WIN32)
    target_link_libraries(crow_app PRIVATE ws2_32)
endif()
