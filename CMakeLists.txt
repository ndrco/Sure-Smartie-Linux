cmake_minimum_required(VERSION 3.24)

project(SureSmartieLinux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

option(SURE_SMARTIE_BUILD_TESTS "Build smoke tests" ON)

find_package(Threads REQUIRED)

add_library(
  sure_smartie_core
  STATIC
  src/core/App.cpp
  src/core/Config.cpp
  src/display/DisplayFactory.cpp
  src/display/StdoutDisplayDriver.cpp
  src/display/SureDisplayDriver.cpp
  src/engine/ScreenManager.cpp
  src/engine/TemplateEngine.cpp
  src/serial/SerialPort.cpp
  providers/BuiltinProviderFactory.cpp
  providers/CpuProvider.cpp
  providers/NetworkProvider.cpp
  providers/RamProvider.cpp
  providers/SystemProvider.cpp
)

target_include_directories(
  sure_smartie_core
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party
)

target_link_libraries(
  sure_smartie_core
  PUBLIC
    Threads::Threads
)

target_compile_options(
  sure_smartie_core
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(sure-smartie-linux src/main.cpp)
target_link_libraries(sure-smartie-linux PRIVATE sure_smartie_core)

if(BUILD_TESTING AND SURE_SMARTIE_BUILD_TESTS)
  add_executable(template_engine_smoke tests/template_engine_smoke.cpp)
  target_link_libraries(template_engine_smoke PRIVATE sure_smartie_core)
  add_test(NAME template_engine_smoke COMMAND template_engine_smoke)
endif()
