cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(SnakeGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Multimedia Test)

set(SOURCES
    src/main.cpp
    src/model/SnakeGameModel.cpp
    src/controller/SnakeGameController.cpp
    src/algorithm/AStar.cpp
    src/view/SnakeGameView.cpp
    src/view/GameOverDialog.cpp
    src/view/LeaderboardDialog.cpp
    resources/resources.qrc
)

set(HEADERS
    src/model/SnakeGameModel.h
    src/controller/SnakeGameController.h
    src/algorithm/AStar.h
    src/view/SnakeGameView.h
    src/view/GameOverDialog.h
    src/view/LeaderboardDialog.h
    src/view/Theme.h
)

add_executable(SnakeGame ${SOURCES} ${HEADERS})

target_include_directories(SnakeGame PRIVATE ${CMAKE_SOURCE_DIR})

# ---- 应用图标（PE 资源） ----
# CMake 内置的 windres 集成在含空格路径下失败：GNU windres 内部以 shell 方式
# 调用 cpp 预处理 .rc 输入，预处理器自身路径（bin 目录含空格）会被拆断，
# 此 binutils 版本亦无 --no-preprocess。
# 解法：
#   1) 在无空格的工作目录复制 rc/ico（输入路径无空格）；
#   2) 用 --preprocessor 显式指定无空格路径的 cpp（从 CXX 编译器目录推导）；
#   3) configure 阶段手工 windres 编译，产物 .obj 直接链接进可执行文件。
# 在无空格的工作目录复制 rc/ico（输入路径无空格，windres 内部以 shell 调用 cpp）。
# 优先使用构建目录保证可复现；若构建目录路径含空格（如 "D:\Visual Studio Code\..."）
# 则回退到系统临时目录（通常无空格）。
if(CMAKE_BINARY_DIR MATCHES " ")
    set(RC_WORK_DIR "$ENV{TEMP}/snakes_rc")
else()
    set(RC_WORK_DIR "${CMAKE_BINARY_DIR}/snakes_rc")
endif()
# 统一为正斜杠：反斜杠在生成的 Makefile 中会被转义吞掉
file(TO_CMAKE_PATH "${RC_WORK_DIR}" RC_WORK_DIR)
file(MAKE_DIRECTORY "${RC_WORK_DIR}")
configure_file(resources/app.rc "${RC_WORK_DIR}/app.rc" COPYONLY)
configure_file(resources/app.ico "${RC_WORK_DIR}/app.ico" COPYONLY)
set(APP_RC_OBJ "${RC_WORK_DIR}/app.rc.obj")
if(CMAKE_RC_COMPILER)
    set(WINDRES_EXE "${CMAKE_RC_COMPILER}")
else()
    set(WINDRES_EXE "windres")
endif()
get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
# 优先显式指定 cpp 预处理器（防 bin 目录含空格被 shell 拆断）；
# 若编译器目录下无 cpp.exe（如 CI 用 choco MinGW），则不传参、让 windres 走 PATH 默认
set(WINDRES_PREPROCESSOR "")
if(EXISTS "${MINGW_BIN_DIR}/cpp.exe")
    set(WINDRES_PREPROCESSOR "--preprocessor=${MINGW_BIN_DIR}/cpp.exe")
endif()
if(WINDRES_PREPROCESSOR)
    execute_process(
        COMMAND "${WINDRES_EXE}" "${WINDRES_PREPROCESSOR}"
                -O coff "${RC_WORK_DIR}/app.rc" "${APP_RC_OBJ}"
        RESULT_VARIABLE RC_RESULT
    )
else()
    execute_process(
        COMMAND "${WINDRES_EXE}" -O coff "${RC_WORK_DIR}/app.rc" "${APP_RC_OBJ}"
        RESULT_VARIABLE RC_RESULT
    )
endif()
if(NOT RC_RESULT EQUAL 0)
    message(FATAL_ERROR "windres failed to compile resources/app.rc (exit ${RC_RESULT})")
endif()

target_link_libraries(SnakeGame PRIVATE Qt5::Core Qt5::Widgets Qt5::Multimedia "${APP_RC_OBJ}")

install(TARGETS SnakeGame DESTINATION bin)
install(FILES resources/styles.qss resources/styles_dark.qss resources/styles_amber.qss
        DESTINATION resources)
install(DIRECTORY resources/sounds DESTINATION resources)

# 将音效复制到构建目录（与可执行文件旁，保证运行时可用）
add_custom_command(TARGET SnakeGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources/sounds
        $<TARGET_FILE_DIR:SnakeGame>/resources/sounds)

# ==================== 常驻单元测试 ====================
# 模型层核心逻辑 + 手动模式玩法规则（headless，无需显示窗口）
# 每个测试文件独立可执行（各自带 QTEST_GUILESS_MAIN main 入口）
enable_testing()

set(TEST_COMMON_SOURCES
    src/model/SnakeGameModel.cpp
    src/algorithm/AStar.cpp
)

add_executable(SnakeGameTestsCore tests/TestModelCore.cpp ${TEST_COMMON_SOURCES})
target_include_directories(SnakeGameTestsCore PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(SnakeGameTestsCore PRIVATE Qt5::Core Qt5::Test)

add_executable(SnakeGameTestsManual tests/TestModelManual.cpp ${TEST_COMMON_SOURCES})
target_include_directories(SnakeGameTestsManual PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(SnakeGameTestsManual PRIVATE Qt5::Core Qt5::Test)

add_test(NAME model_core_tests COMMAND SnakeGameTestsCore)
add_test(NAME model_manual_tests COMMAND SnakeGameTestsManual)
