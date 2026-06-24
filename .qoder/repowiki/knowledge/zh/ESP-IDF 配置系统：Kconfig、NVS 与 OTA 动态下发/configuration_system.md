该固件工程采用 ESP-IDF 标准的**分层配置架构**，结合了编译时静态配置（Kconfig）、运行时持久化存储（NVS）以及云端动态下发（OTA/Server）三种机制。

### 1. 核心配置层级

*   **编译时配置 (Build-time):**
    *   **Kconfig 系统**: 通过 `main/Kconfig.projbuild` 定义项目特有的配置项，如开发板型号 (`BOARD_TYPE`)、屏幕类型 (`DISPLAY_LCD_TYPE`)、唤醒词方案 (`WAKE_WORD_TYPE`)、语言选择等。
    *   **SDK Defaults**: 根目录下的 `sdkconfig.defaults` 及其变体（如 `sdkconfig.defaults.esp32s3`）提供了针对不同芯片和场景的默认参数，包括编译器优化、分区表路径、LVGL 裁剪选项等。
    *   **分区表**: `partitions/v1` 和 `partitions/v2` 目录下定义了不同 Flash 容量（4M/8M/16M/32M）的分区布局。

*   **运行时持久化配置 (Runtime Persistent):**
    *   **NVS (Non-Volatile Storage)**: 核心类 `Settings` (`main/settings.cc`) 封装了 ESP-IDF 的 NVS API。它支持按命名空间（Namespace）管理键值对，提供 `GetString`, `GetInt`, `GetBool` 等接口。
    *   **命名空间约定**: 代码中使用了多个固定的命名空间来隔离不同模块的配置：
        *   `board`: 存储设备 UUID 等硬件标识。
        *   `network` / `wifi`: 存储 WiFi 凭证、OTA URL 等。
        *   `mqtt` / `websocket`: 存储服务器连接参数（由 OTA 动态写入）。
        *   `audio`: 存储音量、音频处理参数。
        *   `assets`: 存储资源包下载状态。
        *   `display`: 存储亮度等显示设置。

*   **动态/云端配置 (Dynamic/Cloud):**
    *   **OTA 激活与下发**: `Ota` 类 (`main/ota.cc`) 在设备启动时向服务器请求版本信息。服务器响应不仅包含固件升级 URL，还包含 `mqtt` 和 `websocket` 配置对象。`Ota::CheckVersion` 会将这些 JSON 配置解析并直接写入 NVS 的对应命名空间中，实现“零接触”配网和协议切换。

### 2. 关键文件与逻辑

*   `main/settings.h/cc`: 通用的 NVS 读写封装，支持只读/读写模式，并在析构时自动提交更改。
*   `main/Kconfig.projbuild`: 定义了数百个与硬件适配和功能开关相关的配置项，是构建系统的核心入口。
*   `main/ota.cc`: 负责从云端拉取配置并同步到本地 NVS，是连接云端与本地配置的桥梁。
*   `main/boards/common/board.cc`: 在构造函数中通过 `Settings` 初始化或读取设备 UUID，确保设备身份的唯一性和持久性。

### 3. 开发者准则

*   **新增配置项**: 如果是硬件相关或编译期确定的功能，应在 `Kconfig.projbuild` 中添加；如果是运行时可变的用户偏好或状态，应使用 `Settings` 类存入 NVS。
*   **命名空间隔离**: 使用 `Settings` 时务必选择合适的命名空间，避免键名冲突。例如，网络相关配置应使用 `"network"` 或 `"wifi"`。
*   **云端同步**: 若需支持云端动态修改配置，需在 OTA 响应协议中增加对应字段，并在 `Ota::CheckVersion` 中实现解析与 NVS 写入逻辑。