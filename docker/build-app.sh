#!/bin/bash
set -e

TOOLCHAIN=/opt/r328-toolchain
DEPS=/opt/r328-deps
INSTALL=/output

export PKG_CONFIG_PATH=$DEPS/lib/pkgconfig:$TOOLCHAIN/lib/pkgconfig
export CFLAGS="--sysroot=$TOOLCHAIN -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -Os -I$DEPS/include"
export LDFLAGS="--sysroot=$TOOLCHAIN -L$TOOLCHAIN/lib -L$DEPS/lib"
export PKG_CONFIG_SYSROOT_DIR=$TOOLCHAIN

cd /src
mkdir -p build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-r328.cmake \
    -DCMAKE_FIND_ROOT_PATH=$DEPS \
    -DCMAKE_PREFIX_PATH=$DEPS \
    -DTINA_SDK_PATH=$TOOLCHAIN/.. \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DXZ_TARGET_DEVICE=ON
make -j$(nproc)

mkdir -p $INSTALL/lib $INSTALL/bin
cp xiaozhi-r328 $INSTALL/bin/
${TOOLCHAIN}/bin/arm-openwrt-linux-strip $INSTALL/bin/xiaozhi-r328

# Copy shared libraries needed on device
for lib in libasound libopus libwebsockets libssl libcrypto libcurl libz libpthread libdl libc libm librt libgcc_s; do
    for f in $(ls $TOOLCHAIN/lib/${lib}.so* $DEPS/lib/${lib}.so* 2>/dev/null); do
        cp -L $f $INSTALL/lib/ 2>/dev/null || true
    done
done

echo "=== Build complete ==="
ls -lh $INSTALL/bin/
ls -lh $INSTALL/lib/
echo "=== Binary info ==="
file $INSTALL/bin/xiaozhi-r328
