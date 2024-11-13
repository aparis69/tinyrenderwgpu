cmake_minimum_required(VERSION 3.11)

project(tinyrenderwebgpu)

option(DEV_MODE "Build in dev mode, use resource directory from source tree rather than installation tree." ON)

include(cmake/utils.cmake)
include(cmake/glfw.cmake)

add_subdirectory(src)
