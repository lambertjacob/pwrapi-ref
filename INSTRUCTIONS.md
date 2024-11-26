# Instructions for Project Configuration

## Building Power API

Install the following programs (should be preloaded by StdEnv/2023):

- automake
- autoconf
- openmpi
- hwloc
- gcc

Run the following command:

```bash
./configure --prefix="$(pwd)/build/install" --with-mpi --enable-debug
```

Use the following command to access node CAC071:

```bash
salloc --qos=privileged --partition=reserved -w cac071
```
