cmake_minimum_required(VERSION 3.10)
project(xiaozhi-r328 C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compile flags
add_compile_options(-Wall -Wextra -Werror)

# Device build flag
option(XZ_TARGET_DEVICE "Build for target device (enables ALSA/Opus/SSL)" OFF)
if(XZ_TARGET_DEVICE)
    add_definitions(-DXZ_TARGET_DEVICE)
    add_definitions(-DTF_LITE_STATIC_MEMORY -DTF_LITE_DISABLE_X86_NEON)
    include_directories(${CMAKE_SOURCE_DIR}/deps/include/kissfft)
endif()

# Common include paths
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)

# External deps (for cross-compilation)
if(XZ_TARGET_DEVICE AND DEFINED ENV{XZ_DEPS_PATH})
    include_directories($ENV{XZ_DEPS_PATH}/include)
    link_directories($ENV{XZ_DEPS_PATH}/lib)
endif()

# Source files
set(SOURCES
    src/main.c
    src/config.c
    src/protocol.c
    src/state_machine.c
    src/audio.c
    src/network.c
    src/ota.c
    src/cli.c
    src/wake_word.cpp
    third_party/cJSON.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/frontend.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/frontend_util.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/frontend_io.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/window.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/window_util.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/window_io.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/fft.cc
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/fft_util.cc
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/fft_io.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/kiss_fft_int16.cc
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/filterbank.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/filterbank_util.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/filterbank_io.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/noise_reduction.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/noise_reduction_util.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/noise_reduction_io.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control_util.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/log_scale.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/log_scale_util.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/log_scale_io.c
    ${CMAKE_SOURCE_DIR}/deps/include/tensorflow/lite/experimental/microfrontend/lib/log_lut.c
)

add_executable(xiaozhi-r328 ${SOURCES})

if(XZ_TARGET_DEVICE)
    # TFLM static library (C++ linked as C)
    if(DEFINED ENV{XZ_DEPS_PATH})
        add_library(tflm STATIC IMPORTED)
        set_target_properties(tflm PROPERTIES
            IMPORTED_LOCATION "$ENV{XZ_DEPS_PATH}/lib/libtensorflow-microlite.a"
        )
        target_link_libraries(xiaozhi-r328 asound opus ssl crypto pthread m tflm)
        target_link_libraries(xiaozhi-r328 "-lstdc++")
    else()
        target_link_libraries(xiaozhi-r328 asound opus ssl crypto pthread m)
    endif()
else()
    target_link_libraries(xiaozhi-r328)
endif()

# Test executables (host only, never use XZ_TARGET_DEVICE)
enable_testing()

add_executable(test_config tests/test_config.c src/config.c third_party/cJSON.c)
target_include_directories(test_config PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/third_party
)

add_executable(test_protocol tests/test_protocol.c src/protocol.c third_party/cJSON.c)
target_include_directories(test_protocol PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/third_party
)

add_executable(test_state_machine tests/test_state_machine.c src/state_machine.c src/protocol.c third_party/cJSON.c)
target_include_directories(test_state_machine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/third_party
)

add_test(NAME test_config COMMAND test_config)
add_test(NAME test_protocol COMMAND test_protocol)
add_test(NAME test_state_machine COMMAND test_state_machine)
