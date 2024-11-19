cmake_minimum_required(VERSION 3.11)

project(tinyrenderwebgpu)

option(DEV_MODE "Build in dev mode, use resource directory from source tree rather than installation tree." ON)

include(cmake/utils.cmake)
include(cmake/glfw.cmake)

add_subdirectory(src)

# MSVC post processing
enable_ide_folders()
if (MSVC)
	# Set startup project automatically 
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT App)

	# CMake Predefined Targets
	set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "third_party/CMakePredefinedTargets")
endif()
