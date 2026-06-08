# 测试公共配置
set(CMAKE_AUTOMOC ON)

# 辅助函数：创建一个 Qt Test 可执行文件并注册到 CTest
function(wormvision_add_test test_name)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES LIBS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${test_name} ${ARG_SOURCES})
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${test_name} PRIVATE
        Qt6::Test
        Qt6::Core
        ${ARG_LIBS}
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# === Phase 0 smoke test ===
wormvision_add_test(test_smoke
    SOURCES test_smoke.cpp
)

# === Phase 1: VideoUtils 单元测试 ===
wormvision_add_test(test_video_utils
    SOURCES
        test_video_utils.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/VideoUtils.cpp
)

# === Phase 2: DatabaseManager 单元测试 ===
wormvision_add_test(test_database_manager
    SOURCES
        test_database_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/data/DatabaseManager.cpp
    LIBS Qt6::Sql
)

# === Phase 5: RecordingDiagnostics 单元测试 ===
wormvision_add_test(test_recording_diagnostics
    SOURCES
        test_recording_diagnostics.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/RecordingDiagnostics.cpp
)

# === AppInstanceLock 单元测试：防止多个进程同时抢占相机 ===
wormvision_add_test(test_app_instance_lock
    SOURCES
        test_app_instance_lock.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/AppInstanceLock.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/AppPaths.cpp
)

# === AppPaths 单元测试：自定义保存目录与派生目录 ===
wormvision_add_test(test_app_paths
    SOURCES
        test_app_paths.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/AppPaths.cpp
)

# === Phase 6: VideoLibraryService 集成测试（防 Phase 4/5 bug 回归）===
wormvision_add_test(test_video_library_service
    SOURCES
        test_video_library_service.cpp
        ${CMAKE_SOURCE_DIR}/src/data/VideoLibraryService.cpp
        ${CMAKE_SOURCE_DIR}/src/data/DatabaseManager.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/VideoUtils.cpp
    LIBS Qt6::Sql
)

# === VideoLibraryWidget 集成测试：刷新只显示当前保存目录 ===
wormvision_add_test(test_video_library_widget
    SOURCES
        test_video_library_widget.cpp
        ${CMAKE_SOURCE_DIR}/src/widgets/VideoLibraryWidget.cpp
        ${CMAKE_SOURCE_DIR}/src/data/VideoLibraryService.cpp
        ${CMAKE_SOURCE_DIR}/src/data/DatabaseManager.cpp
        ${CMAKE_SOURCE_DIR}/src/services/CloudService.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/AppPaths.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/VideoUtils.cpp
    LIBS Qt6::Widgets Qt6::Sql Qt6::Network
)
set_tests_properties(test_video_library_widget PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${CMAKE_BINARY_DIR}/platforms"
)
