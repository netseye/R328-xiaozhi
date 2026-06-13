#!/bin/sh
# xiaozhi-r328 is statically linked (musl), no LD_LIBRARY_PATH needed
exec /mnt/UDISK/xiaozhi-r328 -c /mnt/UDISK/config.json
