cmake_minimum_required(VERSION 3.30)
project(vbc2vid CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.70.0 REQUIRED COMPONENTS filesystem iostreams program_options system)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find GStreamer and Cairo for video output
pkg_check_modules(GST REQUIRED IMPORTED_TARGET
    glib-2.0
    cairo>=1.2
    gstreamer-1.0
)
link_directories(${Boost_LIBRARY_DIRS} ${GST_LIBRARY_DIRS})

set(SOURCES
    src/main.cpp
    src/Event.cpp
    src/Styles.cpp
    src/Tree.cpp
    src/VbcReader.cpp
    src/VideoOutput.cpp
)

add_executable(vbcrender ${SOURCES})
target_include_directories(vbcrender PRIVATE src)
target_link_libraries(vbcrender
    PRIVATE
    Boost::boost
    Boost::filesystem
    Boost::iostreams
    Boost::program_options
    Boost::system
    PkgConfig::GST
    Threads::Threads
)

install(TARGETS vbcrender)
