add_library(webgpu-utils STATIC webgpu-utils.h webgpu-utils.cpp)
target_include_directories(webgpu-utils PUBLIC .)
target_link_libraries(webgpu-utils PUBLIC glfw webgpu glfw3webgpu)
