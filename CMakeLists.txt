cmake_minimum_required(VERSION 3.22)
message(STATUS "CMake version: ${CMAKE_VERSION}")
project(balto
  LANGUAGES CXX)

# Options
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
  -Wall -Wextra
  -Wno-unused-parameter -Wno-unused-function -Wno-unused-const-variable
)
add_compile_options(-march=native)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
add_link_options("-fuse-ld=mold")
# set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -lineinfo --expt-relaxed-constexpr")

IF(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  add_compile_options(-O3 -ffast-math)
ENDIF(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)

IF(CMAKE_BUILD_TYPE MATCHES Debug)
  add_link_options(-fsanitize=address)
  add_compile_options(-fsanitize=address)
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)

# CCache

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Dependencies

find_package(OpenMP REQUIRED)

add_subdirectory(external/indicators)

# Common
add_library(common
  src/header.cpp
  src/beam_state.cpp
  src/beam_search.cpp
)
target_include_directories(common PUBLIC
  src)
target_link_libraries(common PUBLIC
  OpenMP::OpenMP_CXX
  indicators::indicators
)
target_precompile_headers(common PRIVATE src/header.hpp)

# Find perms
add_executable(find_perms
  src/find_perms.cpp
)
target_link_libraries(find_perms PUBLIC common)
target_precompile_headers(find_perms REUSE_FROM common)

# Solver
add_executable(solver
  src/main.cpp
)
target_link_libraries(solver PUBLIC common)
target_precompile_headers(solver REUSE_FROM common)

# Visu
add_executable(visu
  src/visu.cpp
)
target_link_libraries(visu PUBLIC common)
target_precompile_headers(visu REUSE_FROM common)

# Greedy
add_executable(greedy
  src/greedy.cpp
)
target_link_libraries(greedy PUBLIC common)
target_precompile_headers(greedy REUSE_FROM common)

