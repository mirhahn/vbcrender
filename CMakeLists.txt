cmake_minimum_required(VERSION 3.2)
project(vbc2vid CXX)

find_package(Boost 1.46.0 REQUIRED COMPONENTS filesystem iostreams program_options)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find GStreamer and Cairo for video output
pkg_check_modules(GST REQUIRED
    gstreamer-1.0
)
pkg_check_modules(CAIRO REQUIRED
    cairo
)
link_directories(${Boost_LIBRARY_DIRS} ${CAIRO_LIBRARY_DIRS} ${GST_LIBRARY_DIRS})

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
    ${Boost_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${GST_INCLUDE_DIRS}
    src
)
target_link_libraries(vbcrender
    PRIVATE
    ${Boost_LIBRARIES}
    ${GST_LIBRARIES}
    ${CAIRO_LIBRARIES}
    Threads::Threads
)
add_dependencies(vbcrender generate_vbc_code)
