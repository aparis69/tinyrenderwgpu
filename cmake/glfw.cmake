cmake_minimum_required(VERSION 3.11)

include(FetchContent)

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw
)

set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE INTERNAL "Generate installation target")

FetchContent_MakeAvailable(glfw)

set_property(TARGET glfw PROPERTY FOLDER "third_party/GLFW3")
set_property(TARGET update_mappings PROPERTY FOLDER "third_party/GLFW3")