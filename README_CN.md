# 基于 ESP32-S3 的智能键盘（附 Android 配置 App）

### 简介
ESP32 系列芯片是乐鑫科技（[Espressif Inc.](https://www.espressif.com)）生产的热门 IoT 芯片。这些芯片集成了 WiFi、蓝牙、USB、UART 等易用接口，非常适合用于制作键盘。本项目灵感来源于 [MK32](https://github.com/Galzai/MK32) 和 [qmk_firmware](https://github.com/qmk/qmk_firmware)，最初从 MK32 分支而来，后移植了 QMK 的 `tmk_core` 与 `quantum` 组件，并对许多部分进行了重写。

键盘采用 60 键设计，以 ESP32-S3 模块为核心，内置 LCD 屏幕、RGB LED，并配有 Android App，可动态配置键位映射和宏。

![img](https://paul356.github.io/images/keyboard_outlook.jpg)

欢迎提交合并请求。如有建议或功能需求，请开 Issue 反馈。

### 功能特性

#### 硬件
- 60 键布局，支持 3 层映射，覆盖全部 104 个标准按键；完整 NKRO 支持
- Kailh Box 轴体与 SDA 风格键帽
- LCD 屏幕，用于显示键盘状态及访问设置
- ESP32-S3 模块，支持 BLE、WiFi 与 USB 连接
- RGB LED 灯效；支持 3.7 V NTC 电池接口

#### 软件
- Android App，可按层查看并动态修改键位映射
- Android App 宏编辑器：支持定义最多 32 个宏，支持特殊按键转义语法
- BLE 多设备支持，可快速切换输入目标（不支持 iOS）
- 通过 WiFi 与 Web 界面进行 OTA 固件更新
- 内置 Web 服务器，用于键盘状态查看与键位管理
- 功能键：`info`、`intro`、`hotspot`、`toggle_nkro`、`clear_bonds`、`toggle_broadcast`
- 开源固件，支持深度自定义

### LCD 界面

键盘右上角配有 LCD 屏幕，默认显示**输入信息界面**（按键次数、大写锁定、WiFi/BLE 状态、电量）。旋转旋钮可进入设置菜单：

```
├── 输入信息
│   ├── 输入信息界面（默认）
│   └── 重置按键计数器
├── 蓝牙设置
│   ├── 启用 / 禁用蓝牙
│   ├── 蓝牙配对界面
│   └── 切换输入目标
├── WiFi 设置
│   ├── 启用 / 禁用 WiFi
│   └── WiFi 配置
└── LED 设置
    ├── 启用 / 禁用 LED
    └── LED 模式选择
```

旋转旋钮（左/右）可在菜单项之间移动，按 **Enter** 确认选择，按 **Esc** 返回上级。菜单激活期间键盘会捕获所有按键输入；多次按 **Esc** 或等待 5 秒可返回输入信息界面。

### Android App

Android App 通过 BLE 连接键盘（启动 App 前，需在 Android 蓝牙设置中将键盘设为**已配对但未连接**状态）。

- **键位映射标签页** — 显示所有层。长按 **同步** 扫描键盘；短按 **同步** 拉取或推送键位映射。**+** 按钮用于保存/恢复键位映射快照。点击某个按键可打开键码编辑对话框。
- **宏标签页** — 定义最多 32 个宏。宏为纯文本，支持转义序列（详见下方 [键码类型 / MA](#键码类型)）。

### 键码类型

| 类型 | 说明 |
|------|------|
| **BS** | 基础键码 — 标准 104 键中的任意键码 |
| **MD** | 修饰键 + 基础键组合（如 Shift+A） |
| **LT** | 层触发：长按激活某层，短按发送基础键码 |
| **TO** | 永久激活某层 |
| **MO** | 按住时临时激活某层（Super/Fn 键的默认行为） |
| **DF** | 设置默认（始终激活的）层 |
| **TG** | 切换某层的开关状态 |
| **MT** | 修饰键触发：长按为修饰键（Ctrl/Shift/Alt/GUI），短按为基础键码 |
| **MA** | 宏键 — 触发 32 个用户自定义宏之一 |
| **FT** | 功能键 — 触发键盘内置功能 |

#### 宏转义语法

宏支持特殊字符输入，可通过转义语法在宏中插入 ALT、SHIFT、CTRL、ENTER、ESC 等特殊按键。

| 序列 | 含义 |
|------|------|
| `\d` | Delete 键 |
| `\b` | Backspace 键 |
| `\t` | Tab 键 |
| `\e` | Esc 键 |
| `\)` | 修饰符组内的字面 `)` |
| `\lshift(...)` | 按住左 Shift 并发送 `...` |
| `\rshift(...)` | 按住右 Shift 并发送 `...` |
| `\lctrl(...)` | 按住左 Ctrl 并发送 `...` |
| `\rctrl(...)` | 按住右 Ctrl 并发送 `...` |
| `\lalt(...)` | 按住左 Alt 并发送 `...` |
| `\ralt(...)` | 按住右 Alt 并发送 `...` |
| `\lgui(...)` | 按住左 GUI 并发送 `...` |
| `\rgui(...)` | 按住右 GUI 并发送 `...` |

序列可以嵌套，例如 `\lctrl(\lalt(\d))` 表示 Ctrl+Alt+Delete。

### 构建流程

固件基于 esp-idf 5.4 构建。为支持多设备同时连接，本项目对 esp-idf 进行了修改，相关 PR 已提交但尚未合并。修改后的 esp-idf 可从此 [分支](https://github.com/paul356/esp-idf/tree/hid-multiple-conn-support) 下载。若已配置好 esp-idf 环境，请执行以下命令构建固件（注：`get_idf` 为进入 esp-idf 环境的 bash 别名）。

```
git clone https://github.com/paul356/esp32_keyboard.git
get_idf
idf.py set-target esp32s3
idf.py build
idf.py flash # 请按住 BOOT 键使 ESP32S3 进入下载模式
```

在 `build` 目录下可找到固件文件 `esp32_keyboard.bin`，将其烧录到 ESP32S3 开发板即可。注意：本项目复用了 ESP32S3 的 TX0/RX0 引脚，默认情况下 UART0 不会输出调试日志。

### 许可证
tmk_core 和 quantum 采用 GPL-2.0 或更高版本授权。新增代码采用 GPL-3.0 或更高版本授权。具体许可证信息请参阅各文件。
