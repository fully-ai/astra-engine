cmake_minimum_required(VERSION 3.20)
project(astra-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(SDL2 REQUIRED)

find_library(EGL_LIBRARY EGL)
find_path(EGL_INCLUDE_DIR EGL/egl.h)

add_library(astra_core STATIC
  src/core/json.cpp
  src/scene/scene.cpp
  src/assets/asset_pipeline.cpp
  src/physics/physics.cpp
  src/scripting/script_vm.cpp
  src/audio/audio.cpp
  src/render/renderer.cpp
  src/editor/editor.cpp
)

target_include_directories(astra_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(astra_core PUBLIC
  OpenGL::GL
  GLEW::GLEW
  SDL2::SDL2
)
if(EGL_LIBRARY AND EGL_INCLUDE_DIR)
  target_include_directories(astra_core PUBLIC ${EGL_INCLUDE_DIR})
  target_link_libraries(astra_core PUBLIC ${EGL_LIBRARY})
  target_compile_definitions(astra_core PUBLIC ASTRA_HAS_EGL=1)
endif()

add_executable(astra-editor src/apps/editor_main.cpp)
target_link_libraries(astra-editor PRIVATE astra_core)

add_executable(astra-sample examples/starlight_collector/main.cpp)
target_link_libraries(astra-sample PRIVATE astra_core)

add_executable(astra-build src/apps/build_cli.cpp)
target_link_libraries(astra-build PRIVATE astra_core)

enable_testing()
find_package(GTest)
if(GTest_FOUND)
  add_executable(astra_tests tests/engine_tests.cpp)
  target_link_libraries(astra_tests PRIVATE astra_core GTest::gtest_main)
  add_test(NAME AstraCoreAndRenderTests COMMAND astra_tests)
endif()
