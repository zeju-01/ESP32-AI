该项目基于 **ESP-IDF (Espressif IoT Development Framework)** 构建，采用其原生的 **IDF Component Manager** 进行第三方库和内部模块的依赖管理。

### 1. 核心管理系统
- **工具**: `idf_component_manager` (通过 `idf.py` 集成)。
- **清单文件**: `main/idf_component.yml` 是主要的依赖声明文件，定义了项目所需的所有外部组件及其版本约束。
- **锁文件**: `dependencies.lock` 记录了所有直接和间接依赖的确切版本、哈希值（`component_hash`）以及来源，确保构建的可复现性。

### 2. 依赖来源与命名空间
项目依赖主要来自以下注册表源：
- **官方组件**: `espressif/*` (如 `esp-sr`, `led_strip`, `esp_codec_dev`)。
- **社区/第三方组件**: 
  - `78/*`: 由用户 `78` 维护的组件，如 `esp-ml307` (4G模组驱动) 和 `esp-wifi-connect`。
  - `lvgl/lvgl`: 图形界面库。
  - `waveshare/*`: 微雪电子提供的硬件驱动。
  - `txp666/*`, `tny-robotics/*` 等其他开源贡献者。
- **本地/托管组件**: `managed_components/` 目录存储了下载后的组件源码，每个子目录包含独立的 `idf_component.yml` 和 `CMakeLists.txt`。

### 3. 版本控制策略
- **语义化版本**: 广泛使用 SemVer 约束，如 `~1.0.0` (补丁更新), `^2.0.3` (次版本更新), `==1.2.0` (精确匹配)。
- **目标平台约束**: 利用 `rules` 字段根据芯片型号动态引入依赖。例如：
  ```yaml
  espressif/esp32-camera:
    version: ^2.1.4
    rules:
    - if: target in [esp32s3]
  ```
- **IDF 版本要求**: 明确指定 `idf: '>=5.5.2'`，确保框架兼容性。

### 4. 关键依赖类别
- **音频处理**: `espressif/esp-sr` (语音识别), `espressif/esp_audio_codec`。
- **显示与UI**: `lvgl/lvgl` (~9.4.0), `espressif/esp_lvgl_port`, 以及各种 LCD/OLED 驱动 (`esp_lcd_*`)。
- **网络连接**: `espressif/mqtt`, `78/esp-wifi-connect`, `78/esp-ml307` (蜂窝网络)。
- **硬件抽象**: `espressif/button`, `espressif/knob`, `espressif/led_strip`。

### 5. 开发者规范
- **添加依赖**: 应在 `main/idf_component.yml` 中声明，而非手动修改 `managed_components`。
- **更新依赖**: 运行 `idf.py reconfigure` 或 `idf.py add-dependency` 来更新 `dependencies.lock`。
- **离线构建**: `managed_components` 目录通常被纳入版本控制或作为缓存，以支持离线环境下的编译。