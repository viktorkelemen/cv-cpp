cmake_minimum_required(VERSION 3.15)

project(cv_sender VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to the path containing JUCE's CMakeLists.txt (e.g. -DJUCE_DIR=/path/to/JUCE).")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_gui_app(cv_sender_app
    PRODUCT_NAME "CV Sender"
    COMPANY_NAME "cv-cpp"
)

add_library(cv_core STATIC
    src/GridModel.cpp
    src/GridModel.h
)

target_include_directories(cv_core PUBLIC src)

target_sources(cv_sender_app
    PRIVATE
        src/Main.cpp
        src/MainComponent.cpp
        src/MainComponent.h
        src/GridComponent.cpp
        src/GridComponent.h
)

target_compile_definitions(cv_sender_app
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(cv_sender_app
    PRIVATE
        cv_core
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_utils
)

enable_testing()

add_executable(grid_model_tests
    tests/GridModelTests.cpp
)

target_link_libraries(grid_model_tests
    PRIVATE
        cv_core
)

add_test(NAME GridModelTests COMMAND grid_model_tests)
