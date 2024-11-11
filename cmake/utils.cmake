# Reproduce the original folder layout in IDE
function(group_source_by_folder)
	foreach(file ${ARGV}) 
		# Get the directory of the source file
		get_filename_component(parent_dir "${file}" DIRECTORY)

		# Remove common directory prefix to make the group
		string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" group "${parent_dir}")

		# Make sure we are using windows slashes
		string(REPLACE "/" "\\" group "${group}")

		# Group into "Source Files" and "Header Files"
		if ("${file}" MATCHES ".*\\.cpp")
		   set(group "Source Files\\${group}")
		elseif("${file}" MATCHES ".*\\.h")
		   set(group "Header Files\\${group}")
		endif()

		source_group("${group}" FILES "${file}")
	endforeach()
endfunction()

macro(enable_multiprocessor_compilation)
	if(MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
	endif(MSVC)
endmacro()

macro(enable_cpp17)
	set(CMAKE_CXX_STANDARD 17)
	if(MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
	endif(MSVC)
endmacro()

function(target_treat_warnings_as_errors target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /WX)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic -Werror)
	endif()
endfunction()