/**
 * @file main.cpp
 * @brief 贪吃蛇游戏主入口
 *
 * 创建Qt应用程序，实例化游戏控制器（自动创建模型和视图），
 * 进入Qt事件循环。
 */

#include <QApplication>
#include <QIcon>
#include "src/controller/SnakeGameController.h"

/**
 * @brief 程序入口函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 应用退出码
 */
int main(int argc, char* argv[])
{
    // 创建Qt应用程序
    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName(QString::fromUtf8("权重优先自动化贪吃蛇"));
    app.setApplicationVersion("1.0.0");

    // 应用图标：窗口标题栏 / 任务栏 / Alt+Tab（已通过 resources.qrc 内嵌进 exe）
    app.setWindowIcon(QIcon(QStringLiteral(":/resources/app.ico")));

    // 创建游戏控制器（内部自动创建模型和视图）
    SnakeGameController controller;

    // 进入Qt事件循环
    return app.exec();
}
