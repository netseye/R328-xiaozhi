#!/bin/bash
set -e

TOOLCHAIN=/opt/arm-linux-musleabihf-cross
TFLM_SRC=/build/tflm
DEPS=/src/deps

CC="${TOOLCHAIN}/bin/arm-linux-musleabihf-gcc"
CXX="${TOOLCHAIN}/bin/arm-linux-musleabihf-g++"
AR="${TOOLCHAIN}/bin/arm-linux-musleabihf-ar"
RANLIB="${TOOLCHAIN}/bin/arm-linux-musleabihf-ranlib"

CFLAGS="-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -Os -DNDEBUG -DTF_LITE_STATIC_MEMORY -DTF_LITE_DISABLE_X86_NEON -DFLATBUFFERS_LOCALE_INDEPENDENT=0"
CXXFLAGS="$CFLAGS -std=c++14 -fno-exceptions -fno-rtti"

if [ ! -d "$TFLM_SRC" ]; then
    echo "=== Downloading tflite-micro ==="
    git clone --depth 1 https://github.com/tensorflow/tflite-micro.git $TFLM_SRC
fi

cd $TFLM_SRC

# Download flatbuffers manually (skip Makefile which needs PIL/numpy)
echo "=== Downloading flatbuffers ==="
FB_DIR=/build/flatbuffers
if [ ! -f "$FB_DIR/include/flatbuffers/flatbuffers.h" ]; then
    git clone --depth 1 --branch v23.5.26 https://github.com/google/flatbuffers.git $FB_DIR
fi
FB_INC="$FB_DIR/include"
echo "Flatbuffers: $FB_INC"

INCLUDES="-I. -I$FB_INC"

echo "=== Compiling TFLM sources ==="
BUILD_DIR=/build/tflm-obj
rm -rf $BUILD_DIR

compile() {
    local src=$1
    local ext="${src##*.}"
    local obj="$BUILD_DIR/${src%.$ext}.o"
    mkdir -p "$(dirname "$obj")"
    if [ "$ext" = "c" ]; then
        $CC $CFLAGS $INCLUDES -w -c "$src" -o "$obj" 2>&1
    else
        $CXX $CXXFLAGS $INCLUDES -w -c "$src" -o "$obj" 2>&1
    fi
}

OBJ_COUNT=0
FAIL_COUNT=0
OBJECTS=""

# Core sources
for src in \
    tensorflow/lite/micro/micro_allocator.cc \
    tensorflow/lite/micro/micro_interpreter.cc \
    tensorflow/lite/micro/micro_interpreter_graph.cc \
    tensorflow/lite/micro/micro_log.cc \
    tensorflow/lite/micro/micro_op_resolver.cc \
    tensorflow/lite/micro/micro_resource_variable.cc \
    tensorflow/lite/micro/micro_time.cc \
    tensorflow/lite/micro/micro_utils.cc \
    tensorflow/lite/micro/system_setup.cc \
    tensorflow/lite/micro/debug_log.cc \
    tensorflow/lite/micro/flatbuffer_utils.cc \
    tensorflow/lite/micro/memory_helpers.cc \
    tensorflow/lite/micro/fake_micro_context.cc \
    tensorflow/lite/micro/micro_context.cc \
    tensorflow/lite/micro/micro_interpreter_context.cc \
    tensorflow/lite/micro/micro_graph.cc \
    tensorflow/lite/micro/compression.cc \
    tensorflow/lite/c/common.c \
    tensorflow/lite/core/api/flatbuffer_conversions.cc \
    tensorflow/lite/core/api/op_resolver.cpp \
    tensorflow/lite/core/api/tensor_utils.cc \
    tensorflow/lite/core/api/error_reporter.cc \
    tensorflow/lite/schema/schema_utils.cc \
    tensorflow/lite/micro/arena_allocator/non_persistent_arena_buffer_allocator.cc \
    tensorflow/lite/micro/arena_allocator/persistent_arena_buffer_allocator.cc \
    tensorflow/lite/micro/arena_allocator/simple_memory_allocator.cc \
    tensorflow/lite/micro/memory_planner/greedy_memory_planner.cc \
    tensorflow/lite/micro/memory_planner/linear_memory_planner.cc \
    tensorflow/lite/micro/memory_planner/micro_memory_planner.cc; do
    if [ -f "$src" ]; then
        if compile "$src"; then
            OBJECTS="$OBJECTS $BUILD_DIR/${src%.cc}.o"
            OBJECTS="${OBJECTS%.c}.o"
            OBJ_COUNT=$((OBJ_COUNT + 1))
        else
            echo "FAIL: $src"
            FAIL_COUNT=$((FAIL_COUNT + 1))
        fi
    fi
done

# All micro kernels
for src in $(find tensorflow/lite/micro/kernels -name "*.cc" -not -name "*test*" -not -name "*benchmark*"); do
    if compile "$src"; then
        OBJECTS="$OBJECTS $BUILD_DIR/${src%.cc}.o"
        OBJ_COUNT=$((OBJ_COUNT + 1))
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done

echo "Compiled: $OBJ_COUNT OK, $FAIL_COUNT failed"

if [ $OBJ_COUNT -lt 50 ]; then
    echo "ERROR: Too few sources compiled"
    exit 1
fi

echo "=== Creating static library ==="
mkdir -p $DEPS/lib
# Rebuild object list from actual files to fix any path issues
OBJ_FILES=$(find $BUILD_DIR -name "*.o")
$AR rcs $DEPS/lib/libtensorflow-microlite.a $OBJ_FILES
$RANLIB $DEPS/lib/libtensorflow-microlite.a

echo "=== Installing headers ==="
rm -rf $DEPS/include
mkdir -p $DEPS/include
cp -r tensorflow $DEPS/include/
mkdir -p $DEPS/include/flatbuffers
cp -r $FB_INC/flatbuffers $DEPS/include/
find $DEPS/include -name "*test*" -delete 2>/dev/null || true
find $DEPS/include -name "*benchmark*" -delete 2>/dev/null || true

echo "=== TFLM build complete ==="
ls -lh $DEPS/lib/libtensorflow-microlite.a
echo "Objects: $(echo $OBJ_FILES | wc -w), Headers: $(find $DEPS/include -name '*.h' | wc -l)"
