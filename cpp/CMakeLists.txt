cmake_minimum_required(VERSION 3.20)
project(lop_examples CXX)

enable_testing()

# Builds the example plugin; copy the resulting DLL to %AppData%\lop\algorithms.
add_library(lop_examples SHARED register.cpp information.cpp height_classifier.cpp remove_classification.cpp point_data.cpp select_object.cpp points_panel.cpp
    camera_path.cpp camera_path_commands.cpp camera_path_texts.cpp)
target_include_directories(lop_examples PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_features(lop_examples PRIVATE cxx_std_20)
# MSVC reads sources and encodes execution strings as UTF-8.
if (MSVC)
    target_compile_options(lop_examples PRIVATE /utf-8)
endif()

# Tests of the camera path model (no lop host needed); run them with ctest.
add_executable(camera_path_tests tests/camera_path_tests.cpp camera_path.cpp)
target_include_directories(camera_path_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_features(camera_path_tests PRIVATE cxx_std_20)
if (MSVC)
    target_compile_options(camera_path_tests PRIVATE /utf-8)
endif()
foreach(test_name pose_at orientation recorder playback json)
    add_test(NAME camera_path_${test_name} COMMAND camera_path_tests ${test_name})
endforeach()
