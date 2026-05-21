cmake_minimum_required(VERSION 3.20)
project(WormVision VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 自动生成
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 包含路径
include_directories(src)

# 查找 Qt 包
find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    Sql
    Network
    Test
)

# 启用测试
enable_testing()

# 源文件 (移除 VideoRecorder)
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/widgets/VideoDisplayWidget.cpp
    src/widgets/CaptureWidget.cpp
    src/widgets/ControlPanelWidget.cpp
    src/services/CameraController.cpp
    src/widgets/VideoLibraryWidget.cpp
    src/data/DatabaseManager.cpp
    src/data/VideoLibraryService.cpp
    src/utils/ThemeManager.cpp
    src/utils/VideoUtils.cpp
    src/utils/RecordingDiagnostics.cpp
    src/utils/AppInstanceLock.cpp
    src/utils/AppPaths.cpp
    src/services/CloudService.cpp
)

# 头文件 (移除 VideoRecorder)
set(HEADERS
    src/mainwindow.h
    src/widgets/VideoDisplayWidget.h
    src/widgets/CaptureWidget.h
    src/widgets/ControlPanelWidget.h
    src/widgets/VideoLibraryWidget.h
    src/services/CameraController.h
    src/data/DatabaseManager.h
    src/data/VideoLibraryService.h
    src/utils/ThemeManager.h
    src/utils/VideoUtils.h
    src/utils/RecordingDiagnostics.h
    src/utils/AppInstanceLock.h
    src/utils/AppPaths.h
)

# 资源文件
set(RESOURCES
    resources/resources.qrc
)

# Windows 平台：将 .ico 嵌入到 exe 的资源段，让资源管理器/任务栏/快捷方式显示图标
if(WIN32)
    list(APPEND RESOURCES resources/wormvision.rc)
endif()

# 创建可执行文件
add_executable(WormVision WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# 链接库
target_link_libraries(WormVision PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
)

# 海康威视 SDK
target_include_directories(WormVision PRIVATE ${CMAKE_SOURCE_DIR}/libs/hikvision/include)
target_link_directories(WormVision PRIVATE ${CMAKE_SOURCE_DIR}/libs/hikvision/bin)
target_link_libraries(WormVision PRIVATE MvCameraControl)

# 复制 DLL 到输出目录 (Windows)
if(WIN32)
    # Qt DLL 部署
    add_custom_command(TARGET WormVision POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::Widgets>
            $<TARGET_FILE:Qt6::Gui>
            $<TARGET_FILE:Qt6::Core>
            $<TARGET_FILE_DIR:WormVision>
    )

    # Copy Hikvision SDK DLLs
    file(GLOB HIK_DLLS "${CMAKE_SOURCE_DIR}/libs/hikvision/bin/*.dll")
    add_custom_command(TARGET WormVision POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${HIK_DLLS}
            $<TARGET_FILE_DIR:WormVision>
    )
endif()

# 安装配置
install(TARGETS WormVision DESTINATION bin)

# ============================================================================
# 测试
# ============================================================================
add_subdirectory(tests)

# [Post-Build] 自动拷贝 Hikvision SDK 的 DLL 到输出目录
if(WIN32)
    add_custom_command(TARGET WormVision POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/libs/hikvision/bin"
        "$<TARGET_FILE_DIR:WormVision>"
        COMMENT "Copying Hikvision Runtime DLLs to output directory..."
    )
endif()
