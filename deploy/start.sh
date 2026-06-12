#!/bin/sh
export LD_LIBRARY_PATH=/mnt/UDISK/lib:$LD_LIBRARY_PATH
exec /mnt/UDISK/xiaozhi-r328 -c /mnt/UDISK/config.json
