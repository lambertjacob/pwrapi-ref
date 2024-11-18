# Instructions for Project Configuration

This is a guide to configure this project.
## Configuring the project

In [.clangd](./.clangd), change the include directory to wherever you want to install Power API. Also do this in Quicksilver.

## Building Power API

Install the following programs (using Homebrew if on MacOS):
- automake
- autoconf
- openmpi
- hwloc
- gcc

Run the following command, adjusting to your system requirements:

```bash
./configure --prefix="/Users/ethansilver/Code/elec498/pwrapi-ref/build/install" --with-hwloc="/usr/local/Cellar/hwloc/2.11.2" --with-mpi --enable-debug
```

If it doesn't work, try setting the environment variable `CC` to `g++-14`. This bypasses Apple Clang.

In the CAC Environment, ensure the following dependencies are loaded through the `module` interface:
- Python 2.7
- GCC 11.*

Command to get into the node from the login node: `salloc --qos=privileged --partition=reserved -w cac071`

When compiling the source code, ensure the `CXXFLAGS="-fpermissive"` flag is set to avoid compilation errors related to static method declarations.
