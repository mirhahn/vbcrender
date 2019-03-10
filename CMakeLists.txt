cmake_minimum_required(VERSION 3.10)
project(vbcrender CXX C)

# Load CMakeRC for shader resources
include(cmake/CMakeRC.cmake)

# Set C++ standard and OpenGL preference
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OpenGL_GL_PREFERENCE "GLVND")

# Find external packages
find_package(Boost 1.46.0 REQUIRED COMPONENTS filesystem iostreams program_options system)
find_package(OpenGL REQUIRED)
find_package(glfw3 3.2 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find GStreamer and Cairo for video output
pkg_check_modules(GST REQUIRED
    glib-2.0
    cairo>=1.2
    gstreamer-1.0
)
link_directories(${Boost_LIBRARY_DIRS} ${GST_LIBRARY_DIRS} ${OPENGL_LIBRARY_DIRS})

# Set subdirectory paths
set(SCRIPT_DIR ${CMAKE_SOURCE_DIR}/tools/gen_scripts)
set(DEPENDENCY_DIR deps)

add_subdirectory(${DEPENDENCY_DIR})
set_source_files_properties(${VBC_GENERATED_FILES} PROPERTIES GENERATED TRUE)

# Add resource library for OpenGL shaders
file(GLOB SHADER_SOURCES src/renderer/opengl/shaders/*)
cmrc_add_resource_library(vbcrender-shader-resources ALIAS vbcrender::shaderrc NAMESPACE shaders WHENCE src/renderer/opengl/shaders/ ${SHADER_SOURCES})

# Add executable target for vbcrender
file(GLOB_RECURSE STATIC_SOURCES src/*.cpp src/*.c)
message(STATUS "${GLAD_SOURCES}")
set(SOURCES
    ${STATIC_SOURCES}
    ${VBC_GENERATED_FILES}
    ${GLAD_SOURCES}
)

add_executable(vbcrender ${SOURCES})
target_include_directories(vbcrender
    PRIVATE
    ${GST_INCLUDE_DIRS}
    ${GLAD_INCLUDE_DIRS}
    src
)
target_link_libraries(vbcrender
    PRIVATE
    ${CMAKE_DL_LIBS}
    ${GST_LIBRARIES}
    Boost::boost
    Boost::filesystem
    Boost::iostreams
    Boost::program_options
    glfw
    OpenGL::OpenGL
    Threads::Threads
    vbcrender::shaderrc
)
add_dependencies(vbcrender generate_vbc_code)
