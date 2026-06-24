该固件工程采用 **LVGL (Light and Versatile Graphics Library)** 作为核心前端图形框架，用于在 ESP32 系列微控制器的 LCD/OLED 屏幕上渲染用户界面。

### 1. 核心系统与工具
- **图形库**: LVGL (v8/v9 兼容层)，通过 `esp_lvgl_port` 组件进行硬件加速和端口适配。
- **主题管理**: 实现了自定义的 `LvglTheme` 和 `LvglThemeManager` 类，支持**亮色/暗色**双主题切换。主题属性包括背景色、文本色、聊天气泡色、边框色及低电量警告色等。
- **资源管线**: 使用 Python 脚本 (`scripts/Image_Converter`, `managed_components/78__xiaozhi-fonts`) 将 PNG/JPG 图片和 TTF 字体转换为 LVGL 专用的 C 数组格式 (`.c`/`.h`)，并嵌入固件或 SPIFFS 分区。

### 2. 关键文件与架构
- **主题定义**: `main/display/lvgl_display/lvgl_theme.h/cc` 定义了 `LvglTheme` 类，管理颜色令牌 (Color Tokens) 和字体资源。
- **显示驱动抽象**: `main/display/lvgl_display/lvgl_display.cc` 负责 LVGL 任务循环、屏幕缓冲管理及状态栏（电池、网络、音量）的实时更新。
- **表情渲染引擎**: `main/display/lvgl_display/robot_face.cc` 实现了两种渲染模式：
  - **Draw Mode**: 使用 LVGL 原生对象（圆角矩形、线条）动态绘制眼睛、眉毛和嘴巴，支持平滑动画。
  - **Image Mode**: 切换预渲染的 PNG/C 数组图片资源，提供更丰富的视觉效果。
- **样式配置**: 大量使用 `lv_obj_set_style_*` API 直接设置对象样式（如透明度 `LV_OPA_TRANSP`、圆角 `radius`、边距 `pad`），未采用 CSS 类名机制，而是通过代码逻辑控制视觉状态。

### 3. 设计约定与规范
- **颜色解析**: 支持 Hex 字符串（如 `#RRGGBB`）到 `lv_color_t` 的运行时解析，允许通过 JSON 配置文件动态换肤。
- **响应式布局**: 基于屏幕尺寸比例（如 `size_ / 4`）计算元素位置和大小，而非固定像素值，以适配不同分辨率的开发板（从 1.14" 到 3.5" 屏幕）。
- **字体管理**: 通过 `LvglFont` 封装 LVGL 字体指针，支持多语言（中文、英文）和图标字体（Font Awesome）的动态加载。
- **动画策略**: 使用 LVGL 定时器 (`lv_timer_t`) 驱动眨眼、说话口型同步和呼吸效果，确保在主事件循环中非阻塞运行。