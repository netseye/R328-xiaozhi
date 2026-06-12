set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TINA_SDK_PATH "$ENV{TINA_SDK_PATH}" CACHE PATH "Tina SDK root")
set(TOOLCHAIN_BIN "${TINA_SDK_PATH}/bin")

set(CMAKE_C_COMPILER "${TOOLCHAIN_BIN}/arm-openwrt-linux-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/arm-openwrt-linux-g++")
set(CMAKE_AR "${TOOLCHAIN_BIN}/arm-openwrt-linux-ar")
set(CMAKE_RANLIB "${TOOLCHAIN_BIN}/arm-openwrt-linux-ranlib")
set(CMAKE_STRIP "${TOOLCHAIN_BIN}/arm-openwrt-linux-strip")

set(CMAKE_FIND_ROOT_PATH "${TINA_SDK_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS "--sysroot=${TINA_SDK_PATH} -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -Os" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "--sysroot=${TINA_SDK_PATH} -L${TINA_SDK_PATH}/lib -static" CACHE STRING "" FORCE)
