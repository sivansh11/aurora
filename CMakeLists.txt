cmake_minimum_required(VERSION 3.10)

project(aurora)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(FETCHCONTENT_QUIET FALSE)

include(FetchContent)

FetchContent_Declare(
  horizon
  GIT_REPOSITORY https://github.com/sivansh11/horizon
  GIT_TAG main
)
FetchContent_MakeAvailable(horizon)

file(GLOB_RECURSE SRC_FILES src/*cpp)

add_executable(aurora ${SRC_FILES})

target_link_libraries(aurora
  PUBLIC horizon
)

target_include_directories(aurora
  PUBLIC src
)

# default built type if CMAKE_BUILD_TYPE is not set
set(DEFAULT_BUILT_TYPE "Debug")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILT_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILT_TYPE}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
