# Instructions for Project Configuration

This is a guide to configure this project.
## Configuring the project

In [.clangd](./.clangd), change the include directory to wherever you want to install Power API. Also do this in Quicksilver.

## Building Power API

Install the following programs (using Homebrew if on MacOS or by loading the modules on the CAC machine):
- automake
- autoconf
- openmpi
- hwloc
- gcc

Run the following command, adjusting to your system requirements:

```bash
./configure --prefix="/Users/ethansilver/Code/elec498/pwrapi-ref/build/install" --with-hwloc="/usr/local/Cellar/hwloc/2.11.2" --with-mpi --enable-debug
```

Use the following command to access node CAC071: `salloc --qos=privileged --partition=reserved -w cac071`
