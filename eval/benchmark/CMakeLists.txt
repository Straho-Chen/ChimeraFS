cmake_minimum_required(VERSION 3.15)
project(benchmark)

set(cur_dir ${CMAKE_CURRENT_SOURCE_DIR})
set(bin_dir ${CMAKE_CURRENT_SOURCE_DIR}/bin/)

message(NOTICE "Starting to build benchmark")
set(CC ${CMAKE_C_COMPILER})
set(CXX ${CMAKE_CXX_COMPILER})

file(MAKE_DIRECTORY ${bin_dir})

add_custom_target(
    fio-clean
    WORKING_DIRECTORY ${cur_dir}/fio-3.38
    COMMAND make clean
    COMMAND rm -rf ${bin_dir}/fio
)

add_custom_target(
    filebench-clean
    WORKING_DIRECTORY ${cur_dir}/filebench-1.5-alpha3
    COMMAND make clean
    COMMAND rm -rf ${bin_dir}/filebench
)

add_custom_target(
    leveldb-clean
    WORKING_DIRECTORY ${cur_dir}/leveldb-1.23
    COMMAND rm -rf ./build
    COMMAND rm -rf ${bin_dir}/leveldb
)

add_custom_target(
    rocksdb-clean
    COMMAND rm -rf ${cur_dir}/rocksdb-10.0.1/build
    COMMAND rm -rf ${bin_dir}/rocksdb
)

add_custom_target(
    fio ALL
    WORKING_DIRECTORY ${cur_dir}/fio-3.38
    COMMAND ./configure --prefix=${bin_dir}/fio
    COMMAND make -j$(nproc)
    COMMAND make install
)

add_custom_target(
    filebench ALL
    WORKING_DIRECTORY ${cur_dir}/filebench-1.5-alpha3
    COMMAND autoreconf -f -i
    COMMAND ./configure --prefix=${bin_dir}/filebench
    COMMAND make -j$(nproc)
    COMMAND make install
)

add_custom_target(
    leveldb ALL
    WORKING_DIRECTORY ${cur_dir}/leveldb-1.23
    COMMAND mkdir -p ${bin_dir}/leveldb
    COMMAND mkdir -p build && cd build &&
    cmake -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_COMPILER=${CC}
    -DCMAKE_CXX_COMPILER=${CXX}
    -DCMAKE_CROSSCOMPILING=1
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DRUN_HAVE_STD_REGEX=0 -DRUN_HAVE_POSIX_REGEX=0 -S ..
    && cmake --build . -j$(nproc)
    COMMAND cp ${cur_dir}/leveldb-1.23/build/db_bench ${bin_dir}/leveldb/dbbench
    DEPENDS
)

file(MAKE_DIRECTORY ${cur_dir}/rocksdb-10.0.1/build)
add_custom_target(
    rocksdb ALL
    WORKING_DIRECTORY ${cur_dir}/rocksdb-10.0.1/build
    COMMAND mkdir -p ${bin_dir}/rocksdb
    COMMAND cmake -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_COMPILER=${CC}
    -DCMAKE_CXX_COMPILER=${CXX}
    -DCMAKE_CROSSCOMPILING=1
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -S
    ..
    COMMAND cmake --build . -j$(nproc)
    COMMAND cp ${cur_dir}/rocksdb-10.0.1/build/librocksdb.so.10.* ${bin_dir}/rocksdb/librocksdb.so.10
    COMMAND cp ${cur_dir}/rocksdb-10.0.1/build/db_bench ${bin_dir}/rocksdb/dbbench
    COMMAND ls ${bin_dir}
    DEPENDS)


add_custom_target(app-clean DEPENDS
    fio-clean
    filebench-clean
    leveldb-clean
    rocksdb-clean
    ${_clean_targets})
