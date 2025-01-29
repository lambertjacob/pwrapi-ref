POWER_LOC="$(pwd)/build/install"
./autogen.sh
./configure --prefix="${POWER_LOC}" --with-mpi --enable-debug
make install
echo 'Compilation process complete, ensure to: export POWER_LOC="$(pwd)/build/install"'