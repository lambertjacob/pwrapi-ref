# Instructions for Project Configuration

## Building Power API

Install the following programs (should be preloaded by StdEnv/2023):

- automake
- autoconf
- openmpi
- hwloc
- gcc

To build PowerAPI and use it in Quicksilver, set the following environment variable:

```bash
export POWER_LOC="$(pwd)/build/install"
```

Then run the configure command with that location for the install.

```bash
./configure --prefix="${POWER_LOC}" --with-mpi --enable-debug
```

## Accessing the CAC Node

Use the following command to access node CAC071:

```bash
salloc --qos=privileged --partition=reserved -w cac071
```
