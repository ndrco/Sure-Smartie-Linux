cmake_minimum_required(VERSION 3.24)

project(SureSmartieExternalPlugin LANGUAGES CXX)

find_package(SureSmartieLinux CONFIG REQUIRED)

add_library(sure_smartie_external_example MODULE example_plugin.cpp)
target_link_libraries(
  sure_smartie_external_example
  PRIVATE
    SureSmartieLinux::sure_smartie_plugin_sdk
)
set_target_properties(sure_smartie_external_example PROPERTIES PREFIX "")
