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
./configure --prefix="/Users/ethansilver/Code/ELEC498/pwrapi-ref/build/install" --with-hwloc="/usr/local/Cellar/hwloc/2.11.2" --with-mpi --enable-debug
```

If it doesn't work, try setting the environment variable `CC` to `g++-14`. This bypasses Apple Clang.
