# CMake generated Testfile for 
# Source directory: /home/re/src/Sure-Smartie-linux
# Build directory: /home/re/src/Sure-Smartie-linux/build-desktop-test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[template_engine_smoke]=] "/home/re/src/Sure-Smartie-linux/build-desktop-test/template_engine_smoke")
set_tests_properties([=[template_engine_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/re/src/Sure-Smartie-linux/CMakeLists.txt;308;add_test;/home/re/src/Sure-Smartie-linux/CMakeLists.txt;0;")
add_test([=[core_services_smoke]=] "/home/re/src/Sure-Smartie-linux/build-desktop-test/core_services_smoke")
set_tests_properties([=[core_services_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/re/src/Sure-Smartie-linux/CMakeLists.txt;312;add_test;/home/re/src/Sure-Smartie-linux/CMakeLists.txt;0;")
add_test([=[plugin_loader_smoke]=] "/home/re/src/Sure-Smartie-linux/build-desktop-test/plugin_loader_smoke" "/home/re/src/Sure-Smartie-linux/build-desktop-test/sure_smartie_demo_plugin.so")
set_tests_properties([=[plugin_loader_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/re/src/Sure-Smartie-linux/CMakeLists.txt;316;add_test;/home/re/src/Sure-Smartie-linux/CMakeLists.txt;0;")
add_test([=[config_validation_smoke]=] "/home/re/src/Sure-Smartie-linux/build-desktop-test/sure-smartie-linux" "--config" "/home/re/src/Sure-Smartie-linux/configs/stdout-example.json" "--validate-config")
set_tests_properties([=[config_validation_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/re/src/Sure-Smartie-linux/CMakeLists.txt;320;add_test;/home/re/src/Sure-Smartie-linux/CMakeLists.txt;0;")
add_test([=[backlight_command_smoke]=] "/home/re/src/Sure-Smartie-linux/build-desktop-test/sure-smartie-linux" "--config" "/home/re/src/Sure-Smartie-linux/configs/stdout-example.json" "--backlight" "off")
set_tests_properties([=[backlight_command_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/re/src/Sure-Smartie-linux/CMakeLists.txt;327;add_test;/home/re/src/Sure-Smartie-linux/CMakeLists.txt;0;")
add_test([=[gui_smoke]=] "/home/re/src/Sure-Smartie-linux/build-desktop-test/gui_smoke")
set_tests_properties([=[gui_smoke]=] PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" WORKING_DIRECTORY "/home/re/src/Sure-Smartie-linux" _BACKTRACE_TRIPLES "/home/re/src/Sure-Smartie-linux/CMakeLists.txt;345;add_test;/home/re/src/Sure-Smartie-linux/CMakeLists.txt;0;")
