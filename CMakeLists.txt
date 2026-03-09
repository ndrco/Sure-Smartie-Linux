cmake_minimum_required(VERSION 3.24)

project(SureSmartieLinux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(SURE_SMARTIE_BUILD_TESTS "Build smoke tests" ON)
set(SURE_SMARTIE_SYSTEMD_UNIT_DIR "lib/systemd/system" CACHE STRING
    "Install directory for systemd unit files")
set(SURE_SMARTIE_SYSUSERS_DIR "${CMAKE_INSTALL_LIBDIR}/sysusers.d" CACHE STRING
    "Install directory for sysusers.d files")
set(SURE_SMARTIE_SERVICE_USER "_sure-smartie" CACHE STRING
    "User for the systemd service")
set(SURE_SMARTIE_SERVICE_GROUP "_sure-smartie" CACHE STRING
    "Primary group for the systemd service")
set(SURE_SMARTIE_SERIAL_GROUP "dialout" CACHE STRING
    "Supplementary group that grants serial device access")

find_package(Threads REQUIRED)

configure_file(
  packaging/systemd/sure-smartie-linux.service.in
  ${CMAKE_CURRENT_BINARY_DIR}/sure-smartie-linux.service
  @ONLY
)

configure_package_config_file(
  cmake/SureSmartieLinuxConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/SureSmartieLinuxConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SureSmartieLinux
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/SureSmartieLinuxConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

add_library(
  sure_smartie_core
  STATIC
  src/core/App.cpp
  src/core/Config.cpp
  src/core/Logger.cpp
  src/display/DisplayFactory.cpp
  src/display/StdoutDisplayDriver.cpp
  src/display/SureDisplayDriver.cpp
  src/engine/ScreenManager.cpp
  src/engine/TemplateEngine.cpp
  src/plugins/PluginLoader.cpp
  src/serial/SerialPort.cpp
  providers/BuiltinProviderFactory.cpp
  providers/CpuProvider.cpp
  providers/GpuProvider.cpp
  providers/NetworkProvider.cpp
  providers/RamProvider.cpp
  providers/SystemProvider.cpp
)

add_library(sure_smartie_plugin_sdk INTERFACE)
target_include_directories(
  sure_smartie_plugin_sdk
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(sure_smartie_plugin_sdk INTERFACE cxx_std_20)

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
    ${CMAKE_DL_LIBS}
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
target_compile_definitions(
  sure-smartie-linux
  PRIVATE
    SURE_SMARTIE_DEFAULT_CONFIG_PATH="${CMAKE_INSTALL_FULL_SYSCONFDIR}/sure-smartie-linux/config.json"
)

add_library(
  sure_smartie_demo_plugin
  MODULE
  plugins/sure_smartie_demo_plugin.cpp
)
target_include_directories(
  sure_smartie_demo_plugin
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_compile_options(
  sure_smartie_demo_plugin
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
set_target_properties(sure_smartie_demo_plugin PROPERTIES PREFIX "")

install(TARGETS sure-smartie-linux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  TARGETS sure_smartie_plugin_sdk
  EXPORT SureSmartieLinuxPluginSDKTargets
)
install(
  TARGETS sure_smartie_demo_plugin
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/sure-smartie-linux/plugins
)
install(
  DIRECTORY include/sure_smartie
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
  FILES
    configs/sure-example.json
  DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/sure-smartie-linux
  RENAME config.json.example
)
install(
  FILES
    configs/stdout-example.json
    configs/plugin-example.json
    README.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/sure-smartie-linux/examples
)
install(
  DIRECTORY sdk/example-plugin/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/sure-smartie-linux/sdk/example-plugin
)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/sure-smartie-linux.service
  DESTINATION ${SURE_SMARTIE_SYSTEMD_UNIT_DIR}
)
install(
  FILES packaging/sysusers/sure-smartie-linux.conf
  DESTINATION ${SURE_SMARTIE_SYSUSERS_DIR}
)
install(
  EXPORT SureSmartieLinuxPluginSDKTargets
  FILE SureSmartieLinuxPluginSDKTargets.cmake
  NAMESPACE SureSmartieLinux::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SureSmartieLinux
)
install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SureSmartieLinuxConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SureSmartieLinuxConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SureSmartieLinux
)

if(BUILD_TESTING AND SURE_SMARTIE_BUILD_TESTS)
  add_executable(template_engine_smoke tests/template_engine_smoke.cpp)
  target_link_libraries(template_engine_smoke PRIVATE sure_smartie_core)
  add_test(NAME template_engine_smoke COMMAND template_engine_smoke)

  add_executable(plugin_loader_smoke tests/plugin_loader_smoke.cpp)
  target_link_libraries(plugin_loader_smoke PRIVATE sure_smartie_core)
  add_test(
    NAME plugin_loader_smoke
    COMMAND plugin_loader_smoke $<TARGET_FILE:sure_smartie_demo_plugin>
  )
  add_test(
    NAME config_validation_smoke
    COMMAND
      $<TARGET_FILE:sure-smartie-linux>
      --config ${PROJECT_SOURCE_DIR}/configs/stdout-example.json
      --validate-config
  )
endif()
