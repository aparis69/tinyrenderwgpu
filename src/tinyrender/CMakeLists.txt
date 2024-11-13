set(SRC
	include/tinyrender.h
	src/tinyrender.cpp
)

add_library(TinyRender STATIC ${SRC})

target_include_directories(TinyRender 
	PRIVATE 
	include
)

target_link_libraries(TinyRender
	glfw
	webgpu
	glfw3webgpu
	glm_static
	webgpu-utils
)

target_include_directories(TinyRender INTERFACE tinyrender ./include/)

if (DEV_MODE)
	target_compile_definitions(TinyRender PRIVATE -DRESOURCES_DIR="${CMAKE_SOURCE_DIR}/resources")
else (DEV_MODE)
	target_compile_definitions(TinyRender PRIVATE -DRESOURCES_DIR="./resources")
endif (DEV_MODE)

enable_cpp17()
enable_multiprocessor_compilation()
target_treat_warnings_as_errors(TinyRender)
group_source_by_folder(${SRC})