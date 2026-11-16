cmake_minimum_required(VERSION 3.20)
project(lop_examples CXX)

# Builds the example plugin; copy the resulting DLL to %AppData%\lop\algorithms.
add_library(lop_examples SHARED register.cpp information.cpp height_classifier.cpp remove_classification.cpp point_data.cpp select_object.cpp points_panel.cpp)
target_include_directories(lop_examples PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_features(lop_examples PRIVATE cxx_std_20)
# MSVC reads sources and encodes execution strings as UTF-8.
if (MSVC)
    target_compile_options(lop_examples PRIVATE /utf-8)
endif()