该固件工程基于 ESP-IDF 框架，采用了一套结合底层断言、异步事件通知与多模态用户反馈的错误处理体系。

### 1. 核心策略：分层处理
*   **底层硬件/系统层**：依赖 ESP-IDF 原生的 `esp_err_t` 返回码和 `ESP_ERROR_CHECK` 宏。在关键初始化阶段（如 NVS 闪存、GPIO），若遇到不可恢复错误，直接触发系统 Panic/Reset。
*   **应用逻辑层**：采用“异步事件驱动”模式。底层模块（如协议栈、OTA）捕获错误后，不直接阻断线程，而是通过 `SetError()` 或 `xEventGroupSetBits` 发送 `MAIN_EVENT_ERROR` 等信号，由主循环统一调度 UI 和音频反馈。
*   **网络容错**：实现了指数退避重试（Exponential Backoff）和自动重连机制，针对 MQTT/WebSocket 连接失败提供多次尝试机会，避免瞬时网络波动导致服务中断。

### 2. 关键实现细节
*   **错误传播路径**：
    1.  **检测**：`Protocol` 子类（MQTT/WebSocket）在连接或发送失败时调用 `SetError(message)`。
    2.  **通知**：`SetError` 触发 `on_network_error_` 回调，该回调在 `Application::InitializeProtocol` 中被绑定为设置 `last_error_message_` 并置位 `MAIN_EVENT_ERROR`。
    3.  **呈现**：`Application::Run` 主循环检测到事件位后，调用 `Alert()` 方法，同时在屏幕显示错误图标/文字，并播放特定的 OGG 提示音（如 `OGG_EXCLAMATION`）。
*   **资源保护**：在 OTA 升级、音频编解码器初始化等高风险操作中，严格检查返回值（如 `esp_opus_enc_open`），并在失败时记录 `ESP_LOGE` 日志，防止空指针引用。
*   **状态机约束**：`DeviceStateMachine` 确保设备在错误发生后能安全回退到 `kDeviceStateIdle` 状态，保证系统始终处于可控的已知状态。

### 3. 开发者规范
*   **禁止静默失败**：所有涉及硬件 IO、网络请求和内存分配的操作必须检查返回值，并使用 `ESP_LOGE` 记录上下文。
*   **UI 线程安全**：严禁在底层回调（如网络接收线程）中直接操作 LVGL 显示组件。必须通过 `Application::Schedule` 将错误呈现任务投递到主线程执行。
*   **错误本地化**：用户可见的错误信息应引用 `assets/lang_config.h` 中的多语言常量（如 `Lang::Strings::ERROR`），而非硬编码字符串。