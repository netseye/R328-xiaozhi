# XiaoZhi R328

小智 AI 语音助手客户端，运行在全志 R328 嵌入式 Linux 开发板上。

支持语音对话、唤醒词检测（Hey Jarvis），通过 WebSocket 连接小智服务器。

## 前置要求

- 全志 R328 开发板（已刷 Linux 固件）
- 电脑上有 [Docker](https://docs.docker.com/get-docker/) 和 [adb](https://developer.android.com/tools/releases/platform-tools)
- USB 连接开发板，adb 可识别设备

```bash
adb devices   # 确认能看到设备
```

## 配置 WiFi 网络

设备通过 `wpa_supplicant` 连接 WiFi。用 adb 配置：

```bash
# 查看当前 WiFi 配置
adb -s <serial> shell "cat /etc/wifi/wpa_supplicant.conf"

# 写入你的 WiFi 信息（替换 SSID 和密码）
adb -s <serial> shell "cat > /etc/wifi/wpa_supplicant.conf << 'EOF'
ctrl_interface=/etc/wifi/sockets
update_config=1

network={
    ssid=\"你的WiFi名称\"
    psk=\"你的WiFi密码\"
    key_mgmt=WPA-PSK
}
EOF"

# 重启 WiFi 连接
adb -s <serial> shell "killall wpa_supplicant; sleep 1; wpa_supplicant -B -i wlan0 -c /etc/wifi/wpa_supplicant.conf"
adb -s <serial> shell "udhcpc -i wlan0"

# 验证网络
adb -s <serial> shell "ping -c 2 api.tenclass.net"
```

也可以用有线网络（如果开发板有网口），插上网线后会自动获取 IP。

## 快速开始

### 1. 构建 Docker 镜像

```bash
cd docker
docker build -f Dockerfile.musl -t xiaozhi-musl .
```

### 2. 编译

```bash
cd docker
docker run --rm -v "$(pwd)/..":/src -v "$(pwd)/../output":/output xiaozhi-musl:latest bash /src/docker/build-musl.sh
```

编译产物在 `output/` 目录下。

### 3. 部署到设备

```bash
# 一键部署
./deploy.sh <adb-serial>

# 或手动部署
adb -s <serial> push output/xiaozhi-r328 /mnt/UDISK/
adb -s <serial> push output/models /mnt/UDISK/models/
adb -s <serial> push config.json /mnt/UDISK/
```

### 4. 运行

```bash
adb -s <serial> shell "cd /mnt/UDISK && ./xiaozhi-r328 -c config.json"
```

启动后输入命令：

| 命令 | 说明 |
|------|------|
| `chat` | 开始语音对话 |
| `stop` | 中断当前对话 |
| `status` | 查看当前状态 |
| `volume 0-31` | 设置音量 |
| `ota` | 重新注册设备 |
| `quit` | 退出 |

空闲时说 "Hey Jarvis" 可自动唤醒。

## 配置

编辑 `config.json`：

- `DEVICE_ID` — 设备 MAC 地址（需唯一）
- `WEBSOCKET_ACCESS_TOKEN` — 留空则首次启动自动通过 OTA 注册
- `WAKE_WORD_OPTIONS` — 唤醒词模型配置
- `AUDIO_DEVICES` — ALSA 音频设备

## 项目结构

```
src/
  main.c          主程序入口
  audio.c         ALSA 音频采集/播放 + Opus 编解码
  network.c       WebSocket 客户端 (OpenSSL)
  protocol.c      小智协议消息构建与解析
  state_machine.c 状态机 (IDLE/CONNECTING/LISTENING/SPEAKING)
  wake_word.cpp   TFLM 唤醒词检测
  ota.c           OTA 设备注册
  cli.c           命令行交互
  config.c        JSON 配置加载
models/
  hey_jarvis.tflite   唤醒词模型
  hey_jarvis.json     模型配置
docker/
  Dockerfile.musl     构建环境
  build-musl.sh       编译脚本
```

## 硬件要求

- 全志 R328 (ARMv7, VFPv3)
- 麦克风（接入 MIC1/MIC2）
- 扬声器（接入 LINEOUT 或 Speaker）
- 网络连接（WiFi 或以太网）

## License

MIT
