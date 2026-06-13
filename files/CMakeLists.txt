cmake_minimum_required(VERSION 3.10)
project(BouncingBallPlatformer CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

add_executable(platformer main.cpp)

target_include_directories(platformer PRIVATE ${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIR})
target_link_libraries(platformer PRIVATE ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})
