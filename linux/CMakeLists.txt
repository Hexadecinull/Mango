cmake_minimum_required(VERSION 3.22)
project(mango_linux C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Reuses native/'s portable decoder+interpreter unchanged (mango_core is a
# plain host-buildable static lib, no NDK needed, see native/CMakeLists.txt)
# instead of forking a copy. See linux/README.md for what this directory
# adds on top: a standalone process, not Android's native bridge.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../native" "${CMAKE_CURRENT_BINARY_DIR}/native")

# Builds and runs on any host; proves mango_core works with zero Android/JNI
# involvement. Not the real test suite (that's native/tests/test_interp.c).
add_executable(mango_linux_selfcheck src/selfcheck.c)
target_link_libraries(mango_linux_selfcheck PRIVATE mango_core)

# Honest stub: takes an ELF path, prints what's not implemented yet, exits
# nonzero. Real ELF loading + syscall thunking isn't here yet, see README.
add_executable(mango_linux_loader src/loader.c)
target_link_libraries(mango_linux_loader PRIVATE mango_core)
