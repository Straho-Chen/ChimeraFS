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
    fio ALL
    WORKING_DIRECTORY ${cur_dir}/fio-3.38
    COMMAND ./configure --prefix=${bin_dir}/fio
    COMMAND make -j$(nproc)
    COMMAND make install
)

add_custom_target(app-clean DEPENDS 
    fio-clean
${_clean_targets})
