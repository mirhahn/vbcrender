# VbcRender -- Video Rendering for Visualization of Branch Cut Algorithms

VbcRender is a simple command line tool that allows users to render videos from [VBCTOOL](https://informatik.uni-koeln.de/ls-juenger/vbctool/) files. It is designed to have a relatively small memory footprint for large trees.

## Getting Started

VbcRender is currently experimental and does not have any binary release packages. This section explains how to check out the Github repository and build the code.

### Prerequisites

VbcRender makes extensive use of the [Cairo](https://www.cairographics.org), [GLib](https://www.gtk.org), and [GStreamer](https://gstreamer.freedesktop.org) libraries for drawing and video encoding. For input, command line argument parsing, and path manipulations, the [Boost](https://www.boost.org) libraries [Filesystem](https://www.boost.org/doc/libs/release/libs/filesystem/), [Iostreams](https://www.boost.org/doc/libs/release/libs/iostreams/), and [Program Options](https://www.boost.org/doc/libs/release/libs/program_options/) are required. [CMake](https://cmake.org) is used as a build system. VBCTOOL is required to build VbcRender and will automatically be downloaded using Wget during the build process.

The required library and program versions are:

```
Boost >= 1.46
Cairo >= 1.2
CMake >= 3.1
GLib >= 2.0
GStreamer >= 1.0
```

### Installing

This section explains step by step how to build VbcRender. This guide assumes a GNU/Linux system and GNU make as the standard build system for CMake.

First, we clone the latest state of the repository.

```
git clone https://github.com/mirhahn/vbcrender.git
```

Then we change into the new working copy's root directory and create a subdirectory called `build`.

```
cd vbcrender
mkdir build
```

Next, we generate build files inside the build directory. For most purposes, it is desirable to build an optimized version without debugging symbols.

```
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
```

For development purposes, it may be preferrable to build a version with debugging symbols and additional debugging code.

```
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

Finally, we invoke `make` to build vbcrender.

```
make
```

This should generate a standalone executable `./vbcrender` that can be moved to a different directory without any repercussions. To see a list of command line arguments, type

```
./vbcrender --help
```

## Built With

* [Boost Filesystem](https://www.boost.org/doc/libs/release/libs/filesystem/)
* [Boost Iostreams](https://www.boost.org/doc/libs/release/libs/iostreams/)
* [Boost Program Options](https://www.boost.org/doc/libs/release/libs/program_options/)
* [CMake](https://cmake.org)
* [VBCTOOL](https://informatik.uni-koeln.de/ls-juenger/vbctool/)
* [Cairo](https://www.cairographics.org)
* [GLib](https://www.gtk.org)
* [GStreamer](https://gstreamer.freedesktop.org)

## Authors

* **Mirko Hahn** - *Initial developer* - [PurpleBooth](https://github.com/mirhahn)

See also the list of [contributors](https://github.com/mirhahn/vbcrender/contributors) who participated in this project.

## License

This project is licensed under the GNU General Public License v3 or later - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments

* The [VBC file format](https://informatik.uni-koeln.de/fileadmin/projects/vbctool/vbcUserManual.ps.gz) was developed by [Sebastian Leipert](https://www.sebastian-leipert.de/) at the [University of Cologne](http://www.uni-koeln.de/)
