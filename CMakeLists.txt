cmake_minimum_required(VERSION 3.2)
project(vbc2vid CXX)

find_package(Boost 1.46.0 REQUIRED COMPONENTS filesystem iostreams program_options)
find_package(PkgConfig REQUIRED)

# Find GStreamer for video output
pkg_check_modules(GST REQUIRED
    gstreamer-1.0
)
link_directories(${GST_LIBRARY_DIRS})

# Set subdirectory paths
set(SCRIPT_DIR ${CMAKE_SOURCE_DIR}/tools/gen_scripts)
set(DEPENDENCY_DIR deps)

add_subdirectory(${DEPENDENCY_DIR})
set_source_files_properties(${VBC_GENERATED_FILES} PROPERTIES GENERATED TRUE)

set(SOURCES
    src/main.cpp
    src/Event.cpp
    src/Tree.cpp
    src/VbcReader.cpp
    src/VideoOutput.cpp
    ${VBC_GENERATED_FILES}
)

add_executable(vbcrender ${SOURCES})
target_include_directories(vbcrender
    PRIVATE
    ${GST_INCLUDE_DIRS}
    src
)
target_link_libraries(vbcrender
    PRIVATE
    ${GST_LIBRARIES}
    SKIA::Static
    Boost::boost
    Boost::filesystem
    Boost::iostreams
    Boost::program_options
)
add_dependencies(vbcrender generate_vbc_code)
