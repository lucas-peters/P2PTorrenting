# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.31

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /opt/homebrew/bin/cmake

# The command to remove a file.
RM = /opt/homebrew/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/lucaspeters/Documents/GitHub/P2PTorrenting

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/lucaspeters/Documents/GitHub/P2PTorrenting/build

# Include any dependencies generated for this target.
include CMakeFiles/p2p_bootstrap.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/p2p_bootstrap.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/p2p_bootstrap.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/p2p_bootstrap.dir/flags.make

CMakeFiles/p2p_bootstrap.dir/codegen:
.PHONY : CMakeFiles/p2p_bootstrap.dir/codegen

CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.o: CMakeFiles/p2p_bootstrap.dir/flags.make
CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.o: /Users/lucaspeters/Documents/GitHub/P2PTorrenting/src/bootstrap/main.cpp
CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.o: CMakeFiles/p2p_bootstrap.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/lucaspeters/Documents/GitHub/P2PTorrenting/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.o"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.o -MF CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.o.d -o CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.o -c /Users/lucaspeters/Documents/GitHub/P2PTorrenting/src/bootstrap/main.cpp

CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.i"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/lucaspeters/Documents/GitHub/P2PTorrenting/src/bootstrap/main.cpp > CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.i

CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.s"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/lucaspeters/Documents/GitHub/P2PTorrenting/src/bootstrap/main.cpp -o CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.s

CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.o: CMakeFiles/p2p_bootstrap.dir/flags.make
CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.o: /Users/lucaspeters/Documents/GitHub/P2PTorrenting/src/bootstrap/bootstrap_node.cpp
CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.o: CMakeFiles/p2p_bootstrap.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/Users/lucaspeters/Documents/GitHub/P2PTorrenting/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.o"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.o -MF CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.o.d -o CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.o -c /Users/lucaspeters/Documents/GitHub/P2PTorrenting/src/bootstrap/bootstrap_node.cpp

CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.i"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/lucaspeters/Documents/GitHub/P2PTorrenting/src/bootstrap/bootstrap_node.cpp > CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.i

CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.s"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/lucaspeters/Documents/GitHub/P2PTorrenting/src/bootstrap/bootstrap_node.cpp -o CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.s

# Object files for target p2p_bootstrap
p2p_bootstrap_OBJECTS = \
"CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.o" \
"CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.o"

# External object files for target p2p_bootstrap
p2p_bootstrap_EXTERNAL_OBJECTS =

p2p_bootstrap: CMakeFiles/p2p_bootstrap.dir/src/bootstrap/main.cpp.o
p2p_bootstrap: CMakeFiles/p2p_bootstrap.dir/src/bootstrap/bootstrap_node.cpp.o
p2p_bootstrap: CMakeFiles/p2p_bootstrap.dir/build.make
p2p_bootstrap: /opt/homebrew/lib/libtorrent-rasterbar.2.0.11.dylib
p2p_bootstrap: /opt/homebrew/lib/libssl.dylib
p2p_bootstrap: /opt/homebrew/lib/libcrypto.dylib
p2p_bootstrap: CMakeFiles/p2p_bootstrap.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/Users/lucaspeters/Documents/GitHub/P2PTorrenting/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable p2p_bootstrap"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/p2p_bootstrap.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/p2p_bootstrap.dir/build: p2p_bootstrap
.PHONY : CMakeFiles/p2p_bootstrap.dir/build

CMakeFiles/p2p_bootstrap.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/p2p_bootstrap.dir/cmake_clean.cmake
.PHONY : CMakeFiles/p2p_bootstrap.dir/clean

CMakeFiles/p2p_bootstrap.dir/depend:
	cd /Users/lucaspeters/Documents/GitHub/P2PTorrenting/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/lucaspeters/Documents/GitHub/P2PTorrenting /Users/lucaspeters/Documents/GitHub/P2PTorrenting /Users/lucaspeters/Documents/GitHub/P2PTorrenting/build /Users/lucaspeters/Documents/GitHub/P2PTorrenting/build /Users/lucaspeters/Documents/GitHub/P2PTorrenting/build/CMakeFiles/p2p_bootstrap.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/p2p_bootstrap.dir/depend

