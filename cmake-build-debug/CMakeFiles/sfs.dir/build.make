# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.15

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /snap/clion/97/bin/cmake/linux/bin/cmake

# The command to remove a file.
RM = /snap/clion/97/bin/cmake/linux/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/will/Desktop/Small-File-System

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/will/Desktop/Small-File-System/cmake-build-debug

# Utility rule file for sfs.

# Include the progress variables for this target.
include CMakeFiles/sfs.dir/progress.make

CMakeFiles/sfs:
	make -C CLION_EXE_DIR=

sfs: CMakeFiles/sfs
sfs: CMakeFiles/sfs.dir/build.make

.PHONY : sfs

# Rule to build all files generated by this target.
CMakeFiles/sfs.dir/build: sfs

.PHONY : CMakeFiles/sfs.dir/build

CMakeFiles/sfs.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/sfs.dir/cmake_clean.cmake
.PHONY : CMakeFiles/sfs.dir/clean

CMakeFiles/sfs.dir/depend:
	cd /home/will/Desktop/Small-File-System/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/will/Desktop/Small-File-System /home/will/Desktop/Small-File-System /home/will/Desktop/Small-File-System/cmake-build-debug /home/will/Desktop/Small-File-System/cmake-build-debug /home/will/Desktop/Small-File-System/cmake-build-debug/CMakeFiles/sfs.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/sfs.dir/depend

