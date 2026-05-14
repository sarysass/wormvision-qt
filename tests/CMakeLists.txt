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
