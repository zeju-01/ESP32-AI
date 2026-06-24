## 1. 核心系统与框架
该项目基于 **ESP-IDF** 框架，采用其原生的日志系统（`esp_log`）作为唯一的日志输出机制。未引入第三方日志库（如 spdlog 或 log4cpp），而是深度依赖 ESP-IDF 提供的轻量级、宏驱动的日志接口。

- **核心头文件**：`<esp_log.h>`
- **日志宏**：使用 `ESP_LOGE`, `ESP_LOGW`, `ESP_LOGI`, `ESP_LOGD`, `ESP_LOGV` 分别对应错误、警告、信息、调试和详细级别。
- **标签机制**：每个源文件通过 `#define TAG "ModuleName"` 定义唯一的日志标签，用于在输出中区分模块来源（如 `TAG "Application"`, `TAG "main"`）。

## 2. 架构与配置策略
日志系统的行为主要通过编译时配置（Kconfig）和少量的运行时 API 进行控制。

### 2.1 全局日志级别
- **默认级别**：在 `sdkconfig.defaults` 中未显式指定 `CONFIG_LOG_DEFAULT_LEVEL`，通常默认为 `INFO` (3) 或 `DEBUG` (4)，具体取决于 ESP-IDF 的默认设置。
- **Bootloader 日志**：在 `sdkconfig.defaults` 中明确关闭了 Bootloader 阶段的日志输出 (`CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y`)，以加快启动速度并减少串口干扰。

### 2.2 局部动态调试
项目采用“按需开启”的调试策略。对于特定复杂模块（如摄像头视频处理 `esp_video.cc` 和 JPEG 解码 `jpeg_to_image.c`），定义了专用的 Kconfig 选项 `CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE`。
- **实现模式**：
  ```c
  #ifdef CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
  #undef LOG_LOCAL_LEVEL
  #define LOG_LOCAL_LEVEL MAX(CONFIG_LOG_DEFAULT_LEVEL, ESP_LOG_DEBUG)
  #endif
  #include <esp_log.h>
  ```
- **运行时覆盖**：在初始化函数中，通过 `esp_log_level_set(TAG, ESP_LOG_DEBUG)` 动态提升特定标签的日志级别，以便在不重新编译的情况下捕获底层硬件交互细节。

## 3. 关键文件分布
- **入口与核心逻辑**：
  - `main/main.cc`: 系统入口，记录 NVS 初始化和应用启动状态。
  - `main/application.cc`: 核心业务逻辑，记录网络状态、协议交互、音频通道开关及唤醒词检测等关键事件。
- **硬件抽象层 (HAL)**：
  - `main/boards/common/esp_video.cc`: 摄像头驱动，包含详细的 V4L2  ioctl 调用日志和帧格式协商记录。
  - `main/boards/*/power_manager.h`: 各开发板的电源管理模块，记录 ADC 采样值和电量计算过程。
- **显示与多媒体**：
  - `main/display/lvgl_display/jpg/jpeg_to_image.c`: JPEG 软/硬解码流程，记录解码器状态和内存分配情况。

## 4. 开发者规范与建议
1. **标签定义**：所有 `.cc` 或 `.c` 文件必须在包含 `esp_log.h` 之前定义 `TAG` 宏，且名称应简洁并具有辨识度（建议使用类名或模块名）。
2. **日志级别选择**：
   - `INFO`: 用于关键状态流转（如网络连接成功、唤醒词触发）。
   - `DEBUG`: 用于高频循环、传感器原始数据或协议报文细节。
   - `WARN/ERROR`: 仅用于非预期行为或导致功能降级的故障。
3. **性能敏感区**：在音频处理或图像编码等高性能要求路径中，应避免在 `ESP_LOGD` 中执行复杂的字符串拼接或格式化操作，建议利用 `LOG_LOCAL_LEVEL` 在编译期裁剪调试代码。
4. **调试开关**：若需调试特定模块，优先在 `Kconfig.projbuild` 中添加对应的 `ENABLE_XXX_DEBUG_MODE` 选项，并参照 `esp_video.cc` 的模式实现局部日志增强。