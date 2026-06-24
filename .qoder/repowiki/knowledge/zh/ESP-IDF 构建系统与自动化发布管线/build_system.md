## 1. 核心构建框架
项目基于 **Espressif ESP-IDF (v5.5.2)** 框架构建，采用 **CMake** 作为底层构建系统。根目录下的 `CMakeLists.txt` 定义了项目名称 `xiaozhi` 和版本号（如 `2.2.4`），并通过 `idf_build_set_property(MINIMAL_BUILD ON)` 优化编译体积。

## 2. 多板型动态配置机制
为了支持数十种硬件开发板，项目在 `main/CMakeLists.txt` 中实现了一套复杂的动态配置逻辑：
- **Kconfig 映射**：通过 `CONFIG_BOARD_TYPE_XXX` 宏定义，将用户选择的板型映射到具体的源码目录（如 `boards/bread-compact-wifi`）。
- **资源差异化**：根据不同板型的屏幕分辨率、字体需求，动态设置 `BUILTIN_TEXT_FONT`、`DEFAULT_EMOJI_COLLECTION` 等编译宏。
- **组件过滤**：针对特定芯片（如 ESP32 vs ESP32-S3）自动剔除不兼容的驱动文件（如 JPEG 解码或特定音频 Codec）。

## 3. 自动化发布脚本 (`scripts/release.py`)
项目提供了一个强大的 Python 脚本用于固件的批量编译与打包：
- **变体管理**：读取各板型目录下的 `config.json`，解析出目标芯片（target）和多个构建变体（builds）。
- **自动合并**：调用 `idf.py merge-bin` 生成包含 Bootloader、分区表和应用程序的单一二进制文件。
- **版本化归档**：自动将生成的固件压缩为 `releases/v{version}_{board_name}.zip`，便于分发。
- **依赖补全**：内置 `_AUTO_SELECT_RULES`，自动处理 Kconfig 中的 `select` 依赖（如开启 BluFi 时自动启用蓝牙相关配置）。

## 4. CI/CD 流水线 (GitHub Actions)
`.github/workflows/build.yml` 实现了智能化的持续集成：
- **增量编译检测**：在 Pull Request 场景下，通过 `git diff` 分析变更文件。若仅修改了特定板型的代码，则只触发该板型的编译；若修改了 `main/` 核心代码或 `common/` 公共库，则触发全量编译。
- **矩阵构建**：利用 GitHub Actions 的 Matrix 策略，并行处理所有受影响的硬件变体。
- **产物上传**：编译成功后，自动将 `merged-binary.bin` 作为 Artifacts 上传。

## 5. 开发者规范
- **新增板型**：需在 `main/boards/` 下创建目录，编写 `config.json` 定义芯片类型和 SDK 配置追加项，并在 `main/CMakeLists.txt` 中添加对应的 `CONFIG_BOARD_TYPE` 分支。
- **资源配置**：音频文件和语言包通过 `EMBED_FILES` 嵌入固件，需确保路径正确以避免链接错误。
- **版本管理**：版本号统一在根目录 `CMakeLists.txt` 的 `PROJECT_VER` 中维护。