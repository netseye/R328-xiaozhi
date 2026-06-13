#!/bin/bash
set -e

# Download musl toolchain if not present
TOOLCHAIN_DIR=/opt/arm-linux-musleabihf-cross
if [ ! -d "$TOOLCHAIN_DIR" ]; then
    echo "=== Setting up musl toolchain ==="
    if [ -f /src/docker/arm-linux-musleabihf-cross.tgz ]; then
        echo "Using bundled toolchain"
        tar xzf /src/docker/arm-linux-musleabihf-cross.tgz -C /opt
    else
        echo "Downloading toolchain"
        wget -q https://musl.cc/arm-linux-musleabihf-cross.tgz
        tar xzf arm-linux-musleabihf-cross.tgz -C /opt
        rm arm-linux-musleabihf-cross.tgz
    fi
    echo "Toolchain ready"
fi

DEPS=/src/deps
INSTALL=/output
TFLM_SRC=/build/tflm
FB_DIR=/build/flatbuffers
GEMMLOWP_DIR=/build/gemmlowp
RUY_DIR=/build/ruy
BUILD_DIR=/build/tflm-obj

CC="${TOOLCHAIN_DIR}/bin/arm-linux-musleabihf-gcc"
CXX="${TOOLCHAIN_DIR}/bin/arm-linux-musleabihf-g++"
AR="${TOOLCHAIN_DIR}/bin/arm-linux-musleabihf-ar"
RANLIB="${TOOLCHAIN_DIR}/bin/arm-linux-musleabihf-ranlib"

export XZ_DEPS_PATH=$DEPS
export MUSL_TOOLCHAIN=$TOOLCHAIN_DIR

# === Download all TFLM dependencies ===
if [ ! -d "$TFLM_SRC" ]; then
    echo "=== Cloning tflite-micro ==="
    git clone --depth 1 https://github.com/tensorflow/tflite-micro.git $TFLM_SRC
fi
if [ ! -f "$FB_DIR/include/flatbuffers/flatbuffers.h" ]; then
    echo "=== Cloning flatbuffers ==="
    git clone --depth 1 --branch v25.9.23 https://github.com/google/flatbuffers.git $FB_DIR
fi
if [ ! -d "$GEMMLOWP_DIR" ]; then
    echo "=== Downloading gemmlowp ==="
    wget -q https://github.com/google/gemmlowp/archive/fda83bdc38b118cc6b56753bd540caa49e570745.zip -O /tmp/gemmlowp.zip
    unzip -q /tmp/gemmlowp.zip -d /tmp
    mv /tmp/gemmlowp-fda83bdc38b118cc6b56753bd540caa49e570745 $GEMMLOWP_DIR
    rm /tmp/gemmlowp.zip
fi
if [ ! -d "$RUY_DIR" ]; then
    echo "=== Downloading ruy ==="
    wget -q https://github.com/google/ruy/archive/54774a7a2cf85963777289193629d4bd42de4a59.zip -O /tmp/ruy.zip
    unzip -q /tmp/ruy.zip -d /tmp
    mv /tmp/ruy-54774a7a2cf85963777289193629d4bd42de4a59 $RUY_DIR
    rm /tmp/ruy.zip
fi

# === Build TFLM ===
if [ ! -f "$DEPS/lib/libtensorflow-microlite.a" ]; then
    echo "=== Building TFLM ==="

    cd $TFLM_SRC

    CFLAGS_T="-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -Os -DNDEBUG -DTF_LITE_STATIC_MEMORY -DTF_LITE_DISABLE_X86_NEON -DFLATBUFFERS_LOCALE_INDEPENDENT=0"
    CXXFLAGS_T="$CFLAGS_T -std=c++14 -fno-exceptions -fno-rtti"
    INCLUDES="-I. -I$FB_DIR/include -I$GEMMLOWP_DIR -I$RUY_DIR"

    rm -rf $BUILD_DIR

    # Collect ALL sources
    SRC_LIST="/tmp/tflm_sources.txt"
    > $SRC_LIST

    # All .cc and .c files except test, benchmark, and platform-specific dirs
    find tensorflow/lite \
        \( -name "*.cc" -o -name "*.c" \) \
        -not -name "*test*" \
        -not -name "*benchmark*" \
        -not -name "*example*" \
        -not -name "*recipe*" \
        -not -name "*tool*" \
        -not -name "*python*" \
        -not -name "*interop*" \
        -not -path "*/ceva/*" \
        -not -path "*/xtensa/*" \
        -not -path "*/cmsis_nn/*" \
        -not -path "*/arc_mli/*" \
        -not -path "*/micro/tools/*" \
        -not -path "*/core/c/c_api_types.cc" \
        -not -path "*/kernels/internal/optimized/*" \
        | sort >> $SRC_LIST

    # Also include compiler/mlir files needed for schema and error reporter
    find tensorflow/compiler/mlir/lite \
        \( -name "*.cc" -o -name "*.c" \) \
        -not -name "*test*" \
        -not -name "*benchmark*" \
        >> $SRC_LIST

    OBJ_COUNT=0
    FAIL_COUNT=0
    FAILED_LIST="/tmp/tflm_failed.txt"
    > $FAILED_LIST
    while IFS= read -r src; do
        [ -f "$src" ] || continue
        ext="${src##*.}"
        obj="$BUILD_DIR/${src%.$ext}.o"
        mkdir -p "$(dirname "$obj")"
        if [ "$ext" = "c" ]; then
            if $CC $CFLAGS_T $INCLUDES -w -c "$src" -o "$obj" 2>&1; then
                OBJ_COUNT=$((OBJ_COUNT + 1))
            else
                echo "FAIL: $src"
                echo "$src" >> $FAILED_LIST
                FAIL_COUNT=$((FAIL_COUNT + 1))
            fi
        else
            if $CXX $CXXFLAGS_T $INCLUDES -w -c "$src" -o "$obj" 2>&1; then
                OBJ_COUNT=$((OBJ_COUNT + 1))
            else
                echo "FAIL: $src"
                echo "$src" >> $FAILED_LIST
                FAIL_COUNT=$((FAIL_COUNT + 1))
            fi
        fi
    done < $SRC_LIST

    echo "TFLM: compiled $OBJ_COUNT sources ($FAIL_COUNT skipped)"

    if [ $OBJ_COUNT -lt 100 ]; then
        echo "ERROR: Too few sources compiled, expected 100+"
        exit 1
    fi

    # Create static lib
    mkdir -p $DEPS/lib
    $AR rcs $DEPS/lib/libtensorflow-microlite.a $(find $BUILD_DIR -name "*.o")
    $RANLIB $DEPS/lib/libtensorflow-microlite.a

    # Install headers
    mkdir -p $DEPS/include
    cp -r tensorflow $DEPS/include/
    mkdir -p $DEPS/include/flatbuffers
    cp -r $FB_DIR/include/flatbuffers $DEPS/include/

    echo "TFLM build: $(ls -lh $DEPS/lib/libtensorflow-microlite.a | awk '{print $5}')"
fi

# === Install headers for xiaozhi-r328 build ===
apt-get update -qq && apt-get install -y -qq libasound2-dev libopus-dev libssl-dev > /dev/null 2>&1

# Copy headers into deps so cmake finds them
mkdir -p $DEPS/include/alsa $DEPS/include/opus $DEPS/include/openssl
cp -r /usr/include/alsa/* $DEPS/include/alsa/ 2>/dev/null || true
cp -r /usr/include/opus/* $DEPS/include/opus/ 2>/dev/null || true
cp -r /usr/include/openssl/* $DEPS/include/openssl/ 2>/dev/null || true
cp -r /usr/include/aarch64-linux-gnu/openssl/* $DEPS/include/openssl/ 2>/dev/null || true
# Copy gemmlowp headers for fixedpoint support
cp -r $GEMMLOWP_DIR/fixedpoint $DEPS/include/ 2>/dev/null || true
cp -r $GEMMLOWP_DIR/internal $DEPS/include/ 2>/dev/null || true

# === Build xiaozhi-r328 ===
cd /src
rm -rf build && mkdir build && cd build

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-musl.cmake \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DXZ_TARGET_DEVICE=ON \
    -DCMAKE_CXX_FLAGS="-DFLATBUFFERS_LOCALE_INDEPENDENT=0 -Wno-unused-parameter -Wno-sign-compare"

make -j$(nproc)

mkdir -p $INSTALL
cp xiaozhi-r328 $INSTALL/
$TOOLCHAIN_DIR/bin/arm-linux-musleabihf-strip $INSTALL/xiaozhi-r328

# Copy model files
mkdir -p $INSTALL/models
cp /src/models/*.tflite $INSTALL/models/ 2>/dev/null || true
cp /src/models/*.json $INSTALL/models/ 2>/dev/null || true

echo "=== Build complete ==="
ls -lh $INSTALL/
file $INSTALL/xiaozhi-r328
