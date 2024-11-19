set(SRC
	main.cpp
)

add_executable(App
	${SRC}
)

target_include_directories(App 
	PRIVATE 
	include
)

target_link_libraries(App
	glfw
	glm_static
	webgpu
	glfw3webgpu
	TinyRender
)

if (DEV_MODE)
	target_compile_definitions(App PRIVATE -DRESOURCES_DIR="${CMAKE_SOURCE_DIR}/resources")
else (DEV_MODE)
	target_compile_definitions(App PRIVATE -DRESOURCES_DIR="./resources")
endif (DEV_MODE)

enable_cpp17()
enable_multiprocessor_compilation()
target_treat_warnings_as_errors(App)
target_copy_webgpu_binaries(App)
target_group_source_by_folder(App)
