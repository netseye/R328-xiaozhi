# Third-party Dependencies

Libraries that must be cross-compiled for R328 (armv7l):

- **cJSON** — drop cJSON.c and cJSON.h into this directory
- **libopus** — cross-compile from https://gitlab.xiph.org/xiph/opus
- **libwebsockets** — cross-compile from https://github.com/warmcat/libwebsockets

Build each with:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-r328.cmake ..
make
```
