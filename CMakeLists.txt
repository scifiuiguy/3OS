cmake_minimum_required(VERSION 3.20)

project(3os_kernel VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(3os_kernel SHARED
  src/threeos_marshaller.cpp
  src/interaction_facade.cpp
  src/topology.cpp
  src/kinematics.cpp
  src/glyph_registry.cpp
  src/workspace.cpp
)

add_library(3os::kernel ALIAS 3os_kernel)

target_include_directories(3os_kernel
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(3os_kernel
  PRIVATE
    THREEOS_BUILDING_DLL
)

if(NOT WIN32)
  set_target_properties(3os_kernel PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
  )
endif()

set_target_properties(3os_kernel PROPERTIES
  OUTPUT_NAME "3os_kernel"
  WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

if(ANDROID)
  target_link_libraries(3os_kernel PRIVATE c++_shared log)
else()
  add_executable(3os_harness
    tests/harness_main.cpp
  )
  target_link_libraries(3os_harness PRIVATE 3os_kernel)
  enable_testing()
  add_test(NAME threeos_harness COMMAND 3os_harness)
endif()
