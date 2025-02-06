# Instructions for Project Configuration

## Building Power API
For default server config, just run `./compile.sh`

Install the following programs (should be preloaded by StdEnv/2023):

- automake
- autoconf
- openmpi
- hwloc
- gcc

In the CAC Environment, ensure the correct Python version is loaded through the `module` interface:

```bash
module load StdEnv/2020
module load python/2.7.18
```

***This has been causing issues recently, I suggest installing python 2.7.18 locally instead.***

To build PowerAPI and use it in Quicksilver, set the following environment variable:

```bash
export POWER_LOC="$(pwd)/build/install"
```

Run autogen:
```bash
./autogen.sh
```

Then run the configure command with that location for the install.

```bash
./configure --prefix="${POWER_LOC}" --with-mpi --enable-debug
```

If using a local Python installation, make sure to point to it:

```bash
./configure --with-python=(Python install path)/bin/python2.7  --prefix="${POWER_LOC}" --with-mpi --enable-debug
```

Then run the following command to compile the build folder:

```bash
make install
```

## Accessing the CAC Node

Use the following command to access node CAC071:

```bash
salloc --qos=privileged --partition=reserved -w cac071
```

## Using and Editor

Since this project uses a lot of dynamic linking, editors get confused by the dependency tree. To account for this, install [Bear](github.com/rizsotto/Bear)
