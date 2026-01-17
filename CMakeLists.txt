cmake_minimum_required(VERSION 3.20)
project(WormVision VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 自动生成
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt 包
find_package(Qt6 REQUIRED COMPONENTS 
    Widgets 
    OpenGLWidgets 
    Sql
    Network
)

# 查找 OpenGL
find_package(OpenGL REQUIRED)

# 查找 OpenCV
find_package(OpenCV REQUIRED)
include_directories("C:/vcpkg/installed/x64-windows/include/opencv4")

# 源文件
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/widgets/VideoDisplayWidget.cpp
    src/widgets/CaptureWidget.cpp
    src/widgets/ControlPanelWidget.cpp
    src/services/CameraController.cpp
    src/services/VideoRecorder.cpp
    src/widgets/VideoLibraryWidget.cpp
    src/data/DatabaseManager.cpp
    src/utils/ThemeManager.cpp
    src/services/CloudService.cpp
)

# 头文件
set(HEADERS
    src/mainwindow.h
    src/widgets/VideoDisplayWidget.h
    src/widgets/CaptureWidget.h
    src/widgets/ControlPanelWidget.h
    src/widgets/VideoLibraryWidget.h
    src/services/CameraController.h
    src/services/VideoRecorder.h
    src/data/DatabaseManager.h
    src/utils/ThemeManager.h
)

# 资源文件
set(RESOURCES
    resources/resources.qrc
)

# 创建可执行文件
add_executable(WormVision WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# 链接库
target_link_libraries(WormVision PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::Sql
    Qt6::Network
    OpenGL::GL
    ${OpenCV_LIBS}
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
            $<TARGET_FILE:Qt6::OpenGLWidgets>
            $<TARGET_FILE:Qt6::Gui>
            $<TARGET_FILE:Qt6::Core>
            $<TARGET_FILE_DIR:WormVision>
    )
endif()

# 安装配置
install(TARGETS WormVision DESTINATION bin)
