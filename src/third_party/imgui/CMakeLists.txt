add_library(imgui STATIC)
target_sources(imgui
    PRIVATE
    imgui.h
    imgui_internal.h
    
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_widgets.cpp
    imgui_tables.cpp
	
	backends/imgui_impl_glfw.h
    backends/imgui_impl_glfw.cpp
	backends/imgui_impl_wgpu.h
    backends/imgui_impl_wgpu.cpp
)

target_include_directories(imgui PUBLIC
    .
)

target_link_libraries(imgui PUBLIC
    glfw
	webgpu
)
