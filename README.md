# ESP32 蓝牙 MP3 播放器 (A2DP Source)

基于 ESP32 的蓝牙音频源设备，可从 SD 卡读取并播放 MP3 文件，通过蓝牙 A2DP 协议将音频流传输到蓝牙耳机或音箱。

## 功能特性

- **SD 卡 MP3 播放**：从 SD 卡读取 MP3 文件并实时解码
- **蓝牙 A2DP 音频流**：通过蓝牙将高质量音频流传输到蓝牙耳机/音箱
- **OLED 显示**：128x32 SSD1306 显示屏显示播放状态、歌曲信息、进度条和音量
- **按钮控制**：支持上一曲、播放/暂停、下一曲等物理按钮控制
- **AVRCP 协议**：支持音量控制和播放状态反馈
- **MP3 解码**：使用 minimp3 库进行高效的 MP3 解码
- **自动播放列表**：自动扫描 SD 卡中的所有 MP3 文件

## 硬件要求

### 必需硬件

| 组件 | 说明 |
|------|------|
| ESP32 开发板 | 任何标准 ESP32 开发板（支持经典蓝牙） |
| SD 卡模块 | SPI 接口的 SD 卡模块 |
| SD 卡 | FAT32 格式，存放 MP3 文件 |
| SSD1306 OLED | 128x32 I2C 接口 OLED 显示屏 |
| 按钮 x3 | 用于播放控制（上一曲、播放/暂停、下一曲） |
| 蓝牙耳机/音箱 | 支持 A2DP 协议的蓝牙音频设备 |

### GPIO 引脚配置

默认引脚配置如下（可在 `main/gpio_config.h` 中修改）：

#### 按钮控制

| 功能 | GPIO |
|------|------|
| 上一曲 (KEY1) | GPIO 26 |
| 播放/暂停 (KEY2) | GPIO 27 |
| 下一曲 (KEY3) | GPIO 22 |
| 音量 + | GPIO 23 |
| 音量 - | GPIO 33 |

按钮应连接到 GND（按下时接地），所有引脚均启用内部上拉电阻，**无需外接电阻**。

#### SD 卡 (SPI)

| 信号 | GPIO |
|------|------|
| MOSI | GPIO 19 |
| MISO | GPIO 5 |
| SCLK | GPIO 18 |
| CS | GPIO 21 |

#### OLED 显示 (I2C)

| 信号 | GPIO |
|------|------|
| SDA | GPIO 16 |
| SCL | GPIO 17 |

OLED 默认 I2C 地址：`0x3C`

### 硬件连接图

```
ESP32                    SD Card Module
-----                    --------------
GPIO 19 (MOSI) -----> MOSI
GPIO 5  (MISO) <----- MISO
GPIO 18 (SCLK) -----> CLK
GPIO 21 (CS)   -----> CS
3.3V           -----> VCC
GND            -----> GND

ESP32                    SSD1306 OLED
-----                    ------------
GPIO 16 (SDA)  <-----> SDA
GPIO 17 (SCL)  -----> SCL
3.3V           -----> VCC
GND            -----> GND

ESP32                    Buttons
-----                    -------
GPIO 26 (KEY1) -----> Prev Button -----> GND
GPIO 27 (KEY2) -----> Play Button -----> GND
GPIO 22 (KEY3) -----> Next Button -----> GND
GPIO 23        -----> Vol+ Button -----> GND
GPIO 33        -----> Vol- Button -----> GND

```

## 软件依赖

- **ESP-IDF**: v4.4 或更高版本
- **minimp3**: MP3 解码库（已集成在项目中）
- **SSD1306 驱动**: OLED 显示驱动（已集成在项目中）

## 快速开始

### 1. 准备 SD 卡

1. 将 SD 卡格式化为 **FAT32** 格式
2. 将 MP3 文件复制到 SD 卡根目录
3. 支持的 MP3 格式：44.1kHz 采样率，立体声，16-bit

### 2. 配置项目

```bash
idf.py menuconfig
```

**必需的配置项**：
- Component config → Bluetooth → [✓] Bluetooth
- Component config → Bluetooth → Bluedroid Options → [✓] Classic Bluetooth
- Component config → Bluetooth → Bluedroid Options → [✓] A2DP

这些配置已在 `sdkconfig.defaults` 中预设好，通常不需要手动修改。

**可选配置项**：
- 在 `main/common.h` 中修改蓝牙设备名称（`LOCAL_DEVICE_NAME`）
- 在 `main/gpio_config.h` 中修改 GPIO 引脚分配

### 3. 构建和烧录

```bash
# 构建项目
idf.py build

# 烧录到 ESP32（替换 PORT 为实际串口）
idf.py -p PORT flash

# 查看日志输出
idf.py -p PORT monitor
```

或使用提供的便捷脚本：

```bash
# 构建
./build.sh

# 烧录并监控
./monitor.sh
```

退出串口监视器：按 `Ctrl+]`

### 4. 连接蓝牙设备

1. 打开蓝牙耳机/音箱的配对模式
2. ESP32 会自动搜索并连接支持 A2DP 的蓝牙设备
3. 连接成功后，OLED 显示屏会显示 "BT: Connected"
4. 音频会自动开始播放

## 使用说明

### 按钮控制

- **播放/暂停**：短按 KEY2 (GPIO 27) 按钮
- **下一曲**：短按 KEY3 (GPIO 22) 按钮
- **上一曲**：短按 KEY1 (GPIO 26) 按钮
- **音量 +**：短按或长按 GPIO 23
- **音量 -**：短按或长按 GPIO 33

### OLED 显示内容

OLED 显示屏（128x32 像素）分为两行：

**第一行**：
- 蓝牙状态（"BT: Disconnected" 或 "BT: Connected"）
- 当前曲目编号/总曲目数
- 歌曲文件名（滚动显示）

**第二行**：
- 播放进度条
- 音量指示器（"Vol: XX%"）

示例显示：
```
BT:OK [02/15] MySong.mp3
[=====>    ] Vol: 75%
```

### 播放列表

- 系统启动时自动扫描 SD 卡中的所有 `.mp3` 文件
- 按字母顺序排序
- 支持连续播放和循环播放
- 曲目切换时自动更新显示

## 项目结构

```
a2dp_source/
├── main/
│   ├── main.c              # 主程序入口
│   ├── common.h            # 公共定义和全局变量
│   ├── gpio_config.h       # GPIO 引脚配置
│   ├── audio_player.c/h    # 音频播放器和 MP3 解码
│   ├── sd_card.c/h         # SD 卡管理和文件扫描
│   ├── oled_display.c/h    # OLED 显示控制
│   ├── button_control.c/h  # 按钮控制处理
│   ├── bt_a2dp.c/h         # A2DP 音频流处理
│   ├── bt_avrcp.c/h        # AVRCP 控制协议
│   ├── bt_gap.c/h          # 蓝牙 GAP (设备发现和连接)
│   ├── bt_app_core.c/h     # 蓝牙应用核心任务
│   ├── ssd1306.c/h         # SSD1306 OLED 驱动
│   ├── i2c.c               # I2C 通信实现
│   ├── spi.c               # SPI 通信实现
│   ├── minimp3.h           # minimp3 解码库
│   └── font8x8_basic.h     # 8x8 字体数据
├── CMakeLists.txt          # CMake 构建配置
├── sdkconfig.defaults      # 默认配置
├── build.sh                # 构建脚本
├── monitor.sh              # 监控脚本
└── README.md               # 本文档
```

## 示例输出

### 系统启动

```
I (327) BT_AV: Own address:[xx:xx:xx:xx:xx:xx]
I (337) SD_CARD: SD card mounted successfully
I (347) SD_CARD: SD card: Name: SD16G, Type: SDHC/SDXC, Speed: 20 MHz
I (357) SD_CARD: SD card: 15193 MB
I (367) SD_CARD: Found 15 MP3 files on SD card
I (377) AUDIO_PLAYER: Audio player initialized
I (387) OLED: OLED display initialized
I (397) BT_AV: Starting device discovery...
```

### 蓝牙连接

```
I (4523) BT_GAP: Device found: [xx:xx:xx:xx:xx:xx] "My Headphones"
I (4533) BT_AV: a2dp connecting to peer: My Headphones
I (5234) BT_AV: a2dp connected
I (5634) BT_AV: a2dp media ready, starting...
I (6134) BT_AV: a2dp media start successfully
```

### MP3 播放

```
I (6234) AUDIO_PLAYER: Now playing: /sdcard/music/song01.mp3
I (6244) AUDIO_PLAYER: MP3 info: 44100Hz, 2ch, 128kbps
I (7234) AUDIO_PLAYER: Playback progress: 15%
```

### 按钮操作

```
I (12345) BUTTON: Next button pressed
I (12355) AUDIO_PLAYER: Skipping to next track
I (12365) AUDIO_PLAYER: Now playing: /sdcard/music/song02.mp3
```

## 故障排除

### SD 卡无法识别

**问题**：日志显示 "Failed to mount SD card" 或 "No card detected"

**解决方案**：
1. 检查 SD 卡是否正确插入
2. 验证 SPI 引脚连接是否正确（参考 GPIO 配置表）
3. 确认 SD 卡格式为 FAT32（不支持 exFAT）
4. 尝试使用不同的 SD 卡（某些卡可能不兼容）
5. 检查供电是否充足（SD 卡需要稳定的 3.3V）

### MP3 播放有杂音或卡顿

**问题**：音频播放不流畅，有爆音或停顿

**解决方案**：
1. 使用高质量的 SD 卡（Class 10 或更高）
2. 确保 MP3 文件格式为 44.1kHz, 16-bit, 立体声
3. 检查 MP3 文件是否损坏（在电脑上播放测试）
4. 降低 MP3 文件的比特率（推荐 128kbps 或 192kbps）
5. 检查电源供应是否稳定

### 蓝牙无法连接

**问题**：ESP32 找不到蓝牙设备或无法配对

**解决方案**：
1. 确认蓝牙设备处于配对模式
2. 确保蓝牙设备支持 A2DP 协议（大多数耳机和音箱都支持）
3. 清除蓝牙设备的配对列表，重新配对
4. 检查日志中的设备发现信息
5. 在 `main/bt_gap.c` 中修改设备过滤条件

### OLED 显示空白

**问题**：OLED 屏幕没有任何显示

**解决方案**：
1. 检查 I2C 引脚连接（SDA=GPIO16, SCL=GPIO17）
2. 验证 OLED 模块供电（3.3V）
3. 确认 OLED I2C 地址（默认 0x3C，有些模块是 0x3D）
4. 使用 I2C 扫描工具确认设备地址
5. 检查日志中的 OLED 初始化信息

### 按钮无响应

**问题**：按下按钮没有反应

**解决方案**：
1. 检查按钮引脚连接（GPIO 26, 27, 22, 23, 33）
2. 确认按钮连接到 GND（按下时接地）
3. 检查按钮是否正常工作（用万用表测试）
4. 在日志中查看按钮事件（设置 LOG_LEVEL 为 DEBUG）
5. 调整软件去抖动参数（在 `button_control.c` 中）

### 音频编解码问题

**重要提示**：
- ESP32 A2DP 使用 **SBC 编码器**将 PCM 数据编码为蓝牙音频流
- PCM 数据格式：**44.1kHz 采样率，双声道，16-bit**
- minimp3 解码器输出的 PCM 数据必须符合上述格式
- 如果 MP3 文件采样率不是 44.1kHz，可能需要重采样

### 内存不足

**问题**：系统崩溃或重启，日志显示内存分配失败

**解决方案**：
1. 减小音频缓冲区大小（在 `audio_player.c` 中）
2. 调整任务堆栈大小
3. 启用 PSRAM（如果硬件支持）
4. 监控内存使用情况（`esp_get_free_heap_size()`）

## 技术说明

### A2DP 协议

高级音频分发协议 (Advanced Audio Distribution Profile) 用于通过蓝牙传输高质量音频流。

- **角色**：此设备作为 A2DP Source（音频源）
- **编解码器**：SBC (Sub-Band Coding)
- **音频格式**：44.1kHz, 2-channel, 16-bit PCM → SBC

### AVRCP 协议

音频/视频远程控制协议 (Audio/Video Remote Control Profile) 用于控制音频播放。

- **支持的功能**：音量控制、播放状态通知
- **控制范围**：0-127 (AVRCP 标准)

### MP3 解码

使用 minimp3 库进行实时 MP3 解码：

- **特点**：轻量级、高效、无需外部依赖
- **解码性能**：在 ESP32 上可流畅解码 128-320kbps MP3
- **内存占用**：约 30KB

## 扩展功能

### 计划中的功能

- [ ] 播放模式选择（顺序、随机、单曲循环）
- [ ] EQ 均衡器调节
- [ ] 支持更多音频格式（AAC, FLAC）
- [ ] 移动应用控制界面
- [ ] 歌词显示
- [ ] 播放列表管理

### 自定义修改

#### 修改蓝牙设备名称

编辑 `main/common.h`：

```c
#define LOCAL_DEVICE_NAME "ESP_A2DP_SRC"  // 改为你想要的名称
```

#### 修改 GPIO 引脚

编辑 `main/gpio_config.h`，修改相应的 GPIO 定义。

#### 调整音频缓冲区

编辑 `main/audio_player.c`，修改 `RINGBUF_SIZE` 常量。

## 参考资料

- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/index.html)
- [ESP32 蓝牙 API 文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/bluetooth/index.html)
- [A2DP 协议规范](https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=457083)
- [minimp3 库](https://github.com/lieff/minimp3)
- [SSD1306 数据手册](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)

## 许可证

本项目基于 ESP-IDF 示例代码，遵循 Unlicense 或 CC0-1.0 许可证。

## 贡献

欢迎提交 Issue 和 Pull Request！

---

**支持的目标芯片**：ESP32

**注意**：此项目仅使用经典蓝牙功能，不支持 BLE (低功耗蓝牙)。
