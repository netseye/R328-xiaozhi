#!/bin/bash
set -e

TOOLCHAIN=/opt/r328-toolchain
PREFIX=/opt/r328-deps
SYSROOT=$TOOLCHAIN

export CC=arm-openwrt-linux-gcc
export CXX=arm-openwrt-linux-g++
export AR=arm-openwrt-linux-ar
export RANLIB=arm-openwrt-linux-ranlib
export STRIP=arm-openwrt-linux-strip
export CFLAGS="--sysroot=$SYSROOT -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -Os"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="--sysroot=$SYSROOT -L$TOOLCHAIN/lib"

mkdir -p $PREFIX/lib $PREFIX/include

echo "=== Building ALSA lib ==="
if [ ! -f $PREFIX/lib/libasound.so ]; then
    wget -q https://www.alsa-project.org/files/pub/lib/alsa-lib-1.2.8.tar.bz2
    tar xjf alsa-lib-1.2.8.tar.bz2
    cd alsa-lib-1.2.8
    ./configure --host=arm-openwrt-linux --prefix=$PREFIX \
        --sysroot=$SYSROOT \
        --disable-python \
        --disable-static \
        CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS"
    make -j$(nproc) && make install
    cd ..
    echo "ALSA lib done"
else
    echo "ALSA lib already built"
fi

echo "=== Building Opus ==="
if [ ! -f $PREFIX/lib/libopus.so ]; then
    wget -q https://downloads.xiph.org/releases/opus/opus-1.3.1.tar.gz
    tar xzf opus-1.3.1.tar.gz
    cd opus-1.3.1
    ./configure --host=arm-openwrt-linux --prefix=$PREFIX \
        --disable-static \
        --disable-doc \
        --disable-extra-programs \
        CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS"
    make -j$(nproc) && make install
    cd ..
    echo "Opus done"
else
    echo "Opus already built"
fi

echo "=== Building libwebsockets ==="
if [ ! -f $PREFIX/lib/libwebsockets.so ]; then
    if [ ! -d libwebsockets ]; then
        git clone --depth 1 --branch v4.3-stable https://github.com/warmcat/libwebsockets.git
    fi
    cd libwebsockets
    mkdir -p build && cd build
    cmake .. \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=arm \
        -DCMAKE_C_COMPILER=arm-openwrt-linux-gcc \
        -DCMAKE_C_FLAGS="$CFLAGS" \
        -DCMAKE_FIND_ROOT_PATH=$PREFIX \
        -DCMAKE_INSTALL_PREFIX=$PREFIX \
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DLWS_WITH_SSL=ON \
        -DLWS_OPENSSL_LIBRARIES="$TOOLCHAIN/lib" \
        -DLWS_OPENSSL_INCLUDE_DIRS="$TOOLCHAIN/include" \
        -DLWS_WITHOUT_TESTAPPS=ON \
        -DLWS_WITHOUT_EXTENSIONS=ON \
        -DLWS_WITH_MINIMAL_EXAMPLES=OFF \
        -DLWS_STATIC_PIC=OFF \
        -DLWS_WITH_SHARED=ON \
        -DLWS_WITH_STATIC=OFF \
        -DLWS_IPV6=OFF \
        -DLWS_WITH_HTTP2=OFF \
        -DLWS_WITH_HTTP_STREAM=OFF \
        -DLWS_WITH_HTTP_PROXY=OFF \
        -DLWS_WITH_FTS=OFF \
        -DLWS_WITH_ACME=OFF \
        -DLWS_WITH_SPAWN_LISTEN=OFF \
        -DLWS_WITH_GENERIC_SESSIONS=OFF
    make -j$(nproc) && make install
    cd ../..
    echo "libwebsockets done"
else
    echo "libwebsockets already built"
fi

echo "=== All dependencies built ==="
echo "Libraries in $PREFIX/lib:"
ls -la $PREFIX/lib/*.so* 2>/dev/null || echo "No shared libs"
ls -la $PREFIX/lib/*.a 2>/dev/null || echo "No static libs"
echo "Headers in $PREFIX/include:"
ls $PREFIX/include/
