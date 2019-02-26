# Find Git and initialize submodules
find_package(Git REQUIRED)

# Find Python for SKIA build scripts
find_package(PythonInterp REQUIRED)

# Determine combined PATH variable for SKIA build
set(SKIA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/skia")
set(DEPOT_TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/depot_tools")
set(SKIA_BUILD_PATH "${DEPOT_TOOLS_DIR}:$ENV{PATH}")
set(VBC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vbctool")
set(ENV{PATH} "${SKIA_BUILD_PATH}")

# Build SKIA
message(STATUS "Updating Git submodules.")
execute_process(COMMAND ${GIT_EXECUTABLE} submodule init WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
execute_process(COMMAND ${GIT_EXECUTABLE} submodule update WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

message(STATUS "Preparing SKIA build.")
execute_process(COMMAND ${PYTHON_EXECUTABLE} tools/git-sync-deps WORKING_DIRECTORY "${SKIA_DIR}")
execute_process(COMMAND gn gen out/Static "--args=is_official_build=true" WORKING_DIRECTORY "${SKIA_DIR}")

message(STATUS "Building SKIA.")
execute_process(COMMAND ninja -C out/Static WORKING_DIRECTORY "${SKIA_DIR}")
execute_process(COMMAND gn desc out/Static //:skia libs WORKING_DIRECTORY "${SKIA_DIR}" OUTPUT_VARIABLE SKIA_EXT_LIBS)
string(REPLACE "\n" ";" SKIA_EXT_LIBS "${SKIA_EXT_LIBS}")

set(SKIA_COMPONENTS config core gpu)
set(SKIA_INCLUDE_DIRS "")
foreach(SKIA_COMPONENT ${SKIA_COMPONENTS})
	list(APPEND SKIA_INCLUDE_DIRS "${SKIA_DIR}/include/${SKIA_COMPONENT}")
endforeach(SKIA_COMPONENT)

add_library(SKIA::Static STATIC IMPORTED GLOBAL)
target_include_directories(SKIA::Static INTERFACE "${SKIA_INCLUDE_DIRS}")
target_link_libraries(SKIA::Static INTERFACE "${SKIA_EXT_LIBS}")
set_target_properties(SKIA::Static PROPERTIES IMPORTED_LOCATION "${SKIA_DIR}/out/Static/libskia.a")
add_dependencies(SKIA::Static build_skia_static)

# Add custom target for VBCTool
file(MAKE_DIRECTORY ${VBC_DIR})
message(STATUS "Downloading VbcTool")
file(DOWNLOAD
	https://informatik.uni-koeln.de/fileadmin/projects/vbctool/vbcLinux.tgz
	${VBC_DIR}/vbcLinux.tgz
)
execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf vbcLinux.tgz
	WORKING_DIRECTORY ${VBC_DIR}
)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/src/Styles.cpp
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src
        COMMAND ${PYTHON_EXECUTABLE} ${SCRIPT_DIR}/generate_vbc_code.py > ${CMAKE_BINARY_DIR}/src/Styles.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        MAIN_DEPENDENCY ${SCRIPT_DIR}/generate_vbc_code.py
	DEPENDS ${VBC_DIR}/vbctool/GRAPHResource/GRAPHrgb.txt
		${VBC_DIR}/vbctool/GRAPHResource/GRAPHStandardResource.rsc
	COMMENT "Generating VBC style code"
)
add_custom_target(generate_vbc_code
	DEPENDS ${CMAKE_BINARY_DIR}/src/Styles.cpp
	COMMENT "Generating VBC Code"
)

set(VBC_GENERATED_FILES "${CMAKE_BINARY_DIR}/src/Styles.cpp")

# Export variables to parent scope
set(DEPOT_TOOLS_DIR "${DEPOT_TOOLS_DIR}" PARENT_SCOPE)
set(SKIA_DIR "${SKIA_DIR}" PARENT_SCOPE)
set(SKIA_INCLUDE_DIRS "${SKIA_INCLUDE_DIRS}" PARENT_SCOPE)
set(VBC_GENERATED_FILES "${VBC_GENERATED_FILES}" PARENT_SCOPE)
