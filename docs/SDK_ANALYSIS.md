# 海康威视 MVS SDK 资源库深度分析报告

## 1. 概述
在移除无关内容后，目前的 `Development` 文件夹构成了本项目的核心 **"真理资源库" (Source of Truth)**。它包含了开发 WormVision Qt 所需的全部依赖、文档和参考实现。

## 2. 核心目录简介与价值

目前的目录结构极其精简，每一部分都至关重要：

| 目录名 | 内容简介 | 项目作用 | 价值评级 |
| :--- | :--- | :--- | :--- |
| **`Includes`** | **头文件** (`.h`)<br>含 `MvCameraControl.h` 等 | **编译基石**<br>定义了所有 API 接口和数据结构，是代码编译的必要条件。 | ⭐⭐⭐⭐⭐ (必须) |
| **`Libraries`** | **静态库** (`.lib`)<br>`win64/MvCameraControl.lib` | **链接基石**<br>提供了 API 的符号入口，是生成可执行文件的必要条件。 | ⭐⭐⭐⭐⭐ (必须) |
| **`Documentations`** | **技术文档**<br>API 手册、参数表、示例说明 | **理论指导**<br>解决 "参数传什么"、"错误码是什么" 的权威查询入口。 | ⭐⭐⭐⭐⭐ (核心) |
| **`Samples/C++`** | **示例源码**<br>官方标准实现代码 | **实践模版**<br>提供 "录像"、"渲染"、"截图" 等功能的工业级标准写法。 | ⭐⭐⭐⭐⭐ (指南) |
| **`Bin`** | **运行时库** (`.dll`)<br>含 `MvCameraControl.dll` | **调试环境**<br>用于快速运行 Samples 中的示例程序进行对比测试。 | ⭐⭐⭐ (工具) |

## 3. 详细资源导览

### 3.1 理论指导 (Documentations)
遇到问题时，请优先查阅以下三个文件：
1.  **`工业相机Windows SDK开发指南V4.6.0（C）.chm`**: API 字典。查函数原型、错误码含义。
2.  **`工业相机SDK可配置化参数表.xlsx`**: 属性地图。查 GenICam 节点名称（如 "TriggerMode"），及其支持的数据类型和取值范围。
3.  **`工业相机Windows SDK C++示例程序说明.pdf`**: 示例导航。帮你快速找到实现特定功能的代码所在的文件夹。

### 3.2 实践模版 (Samples/C++/General)
开发新功能时，请直接由以下代码“抄作业”：
*   **`Recording`**: **录像功能的标准实现**。演示了如何正确填充 `MV_CC_INPUT_FRAME_INFO` 和处理动态参数。
*   **`GrabImage_Display`**: **高性能渲染的教科书**。演示了如何使用窗口句柄 (`HWND`) 进行零拷贝直接显示。
*   **`ImageSave`**: **截图/快照的蓝本**。演示了如何将内存中的裸数据保存为 BMP/JPEG/PNG。

### 3.3 编译依赖 (Includes & Libraries)
*   **配置**: 在 `CMakeLists.txt` 中，已分别通过 `target_include_directories` 指向 `Includes`，通过 `target_link_directories` 指向 `Libraries/win64`。
*   **注意**: 即使在 CI/CD 环境中，这两个文件夹也是构建成功的最低刚需。

## 4. 总结
这个精简后的 `Development` 文件夹不再是杂乱的资料堆，而是**项目的一部分**。
- **Includes/Libraries**: 是项目的**骨架** (Build Dependencies)。
- **Documentations/Samples**: 是项目的**大脑** (Knowledge Base)。

建议将此文件夹纳入版本控制（除 `.git` 忽略的大型二进制外），或作为 submodule 管理，确保所有开发者拥有一致的开发环境。
