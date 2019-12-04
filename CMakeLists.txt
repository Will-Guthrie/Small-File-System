cmake_minimum_required(VERSION 3.15)
project(COMP_310_File_System C)

set(CMAKE_C_STANDARD 99)
add_definitions(-c -g -Wall -D_FILE_OFFSET_BITS=64 -std=gnu99 `pkg-config fuse --cflags --libs`)


add_executable(COMP_310_File_System disk_emu.c sfs_api.c sfs_test2.c sfs_api.h)


