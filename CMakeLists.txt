cmake_minimum_required(VERSION 3.10)
project(xiaozhi-r328 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compile flags
add_compile_options(-Wall -Wextra -Werror)

# Find packages
find_package(ALSA)
find_package(Opus)
find_package(WebSockets)
find_package(cJSON)

# Third-party fallback
if(NOT Opus_FOUND)
    set(OPUS_LIBRARIES "")
    set(OPUS_INCLUDE_DIRS "")
endif()

# Source files
set(SOURCES
    src/main.c
    src/config.c
    src/protocol.c
    src/state_machine.c
    src/audio.c
    src/network.c
    src/cli.c
)

add_executable(xiaozhi-r328 ${SOURCES})

target_include_directories(xiaozhi-r328 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
    ${ALSA_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
    ${WEBSOCKETS_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(xiaozhi-r328
    ${ALSA_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${WEBSOCKETS_LIBRARIES}
    ${CJSON_LIBRARIES}
    ssl
    crypto
    pthread
    m
)

# Test executables
enable_testing()

add_executable(test_config tests/test_config.c src/config.c)
target_include_directories(test_config PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/third_party
)

add_executable(test_protocol tests/test_protocol.c src/protocol.c)
target_include_directories(test_protocol PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

add_executable(test_state_machine tests/test_state_machine.c src/state_machine.c src/protocol.c)
target_include_directories(test_state_machine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

add_test(NAME test_config COMMAND test_config)
add_test(NAME test_protocol COMMAND test_protocol)
add_test(NAME test_state_machine COMMAND test_state_machine)
