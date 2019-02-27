# Find Git and initialize submodules
find_package(Git REQUIRED)

# Determine VBC directory
set(VBC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vbctool")

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
set(VBC_GENERATED_FILES "${VBC_GENERATED_FILES}" PARENT_SCOPE)
