cmake_minimum_required(VERSION 3.22)

project(aurora)

include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_VERBOSE_MAKEFILE ON)

# # deps
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
  horizon
  GIT_REPOSITORY https://github.com/sivansh11/horizon
  GIT_TAG main
)

if (NOT horizon_POPULATED)
  FetchContent_MakeAvailable(horizon)
endif()

# # src
file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_executable(aurora
  ${SRC_FILES}
)

target_link_libraries(aurora
  PUBLIC horizon
)

