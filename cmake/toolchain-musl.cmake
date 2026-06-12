set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(MUSL_TOOLCHAIN "$ENV{MUSL_TOOLCHAIN}" CACHE PATH "musl toolchain root")
set(TOOLCHAIN_BIN "${MUSL_TOOLCHAIN}/bin")

set(CMAKE_C_COMPILER "${TOOLCHAIN_BIN}/arm-linux-musleabihf-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/arm-linux-musleabihf-g++")
set(CMAKE_AR "${TOOLCHAIN_BIN}/arm-linux-musleabihf-ar")
set(CMAKE_RANLIB "${TOOLCHAIN_BIN}/arm-linux-musleabihf-ranlib")
set(CMAKE_STRIP "${TOOLCHAIN_BIN}/arm-linux-musleabihf-strip")

set(CMAKE_FIND_ROOT_PATH "${MUSL_TOOLCHAIN}/arm-linux-musleabihf")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -Os" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "-static" CACHE STRING "" FORCE)
