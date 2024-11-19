include(FetchContent)

function(target_get_sources Target ReturnVar)
	get_target_property(SOURCES ${Target} SOURCES)
	get_target_property(INTERFACE_SOURCES ${Target} INTERFACE_SOURCES)
	set(ALL_SOURCES "")
	if (SOURCES)
		list(APPEND ALL_SOURCES ${SOURCES})
	endif()
	if (INTERFACE_SOURCES)
		list(APPEND ALL_SOURCES ${INTERFACE_SOURCES})
	endif()
	set(SOURCES "${ALL_SOURCES}")
	set(${ReturnVar} ${ALL_SOURCES} PARENT_SCOPE)
endfunction()

# Reproduce the original folder layout in IDE
function(group_source_by_folder prefix)
	foreach(file ${ARGV})
		# Get the directory of the source file
		cmake_path(GET file PARENT_PATH parent_dir)

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

		source_group("${prefix}/${group}" FILES "${file}")
	endforeach()
endfunction()

function(target_set_rec_source_group Target Group)
	get_target_property(Dependencies ${Target} LINK_LIBRARIES)
	if (Dependencies)
		foreach(Dep ${Dependencies})
			if (TARGET ${Dep})
				target_get_sources(${Dep} SRC)
				source_group(${Group} FILES ${SRC})
				target_set_rec_source_group(${Dep} ${Group})
			endif()
		endforeach()
	endif()
endfunction()

function(target_group_source_by_folder Target)
	target_get_sources(${Target} SRC)
	group_source_by_folder("" ${SRC})
	target_set_rec_source_group(${Target} "Dependencies")
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

function(enable_ide_folders)
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endfunction()

function(target_treat_warnings_as_errors target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /WX)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic -Werror)
	endif()
endfunction()

macro(targets_set_folder all_targets folder)
	foreach(target IN ITEMS ${${all_targets}})
		if (TARGET ${target})
			set_target_properties(${target} PROPERTIES FOLDER "${folder}")
		endif()
	endforeach()
endmacro()