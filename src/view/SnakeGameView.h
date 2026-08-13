/**
 * @file SnakeGameView.h
 * @brief 贪吃蛇游戏视图层
 *
 * 负责渲染25×25网格、渐变色蛇身、带权重的圆形食物、
 * 游戏状态信息标签以及控制按钮（开始/暂停/重置）。
 * 通过Qt信号槽与Model层交互。
 */

#ifndef SNAKEGAMEVIEW_H
#define SNAKEGAMEVIEW_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <memory>

// 前向声明
class SnakeGameModel;

// 界面主题枚举（与 resources/ 下 QSS 文件一一对应）
#include "src/view/Theme.h"

// UI 像素常量：网格尺寸来自 Model（GRID_SIZE），像素大小属视图职责
const int CELL_SIZE = 28;

/**
 * @brief 游戏视图类
 *
 * 继承QWidget，使用QPainter绘制游戏画面。
 * 管理所有UI控件的布局和事件响应。
 */
class SnakeGameView : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param model 游戏模型的共享指针
     * @param parent 父窗口
     */
    explicit SnakeGameView(std::shared_ptr<SnakeGameModel> model, QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~SnakeGameView() override;

    /// 获取当前界面主题（Controller 传参给弹窗使用）
    ViewTheme getTheme() const { return m_theme; }

    /**
     * @brief 切换界面主题（应用对应 QSS + QSettings 持久化）
     */
    void setTheme(ViewTheme theme);

    /**
     * @brief 更新音效开关按钮文案（Controller 切换音效后回调）
     * @param enabled 音效是否开启
     */
    void setSoundEnabled(bool enabled);

signals:
    /// ESC 快捷键按下（Controller 连接为暂停/恢复）
    void escapePressed();

    /// 点击排行榜按钮（Controller 连接为弹出排行榜）
    void leaderboardRequested();

    /// 点击音效开关按钮（Controller 连接为切换音效）
    void soundToggleRequested();

protected:
    /**
     * @brief 重绘事件 - 绘制游戏画面
     * @param event 绘制事件参数
     */
    void paintEvent(QPaintEvent* event) override;

private slots:
    /// 处理"开始"按钮点击
    void onStartClicked();

    /// 处理"暂停"按钮点击
    void onPauseClicked();

    /// 处理"重置"按钮点击
    void onResetClicked();

    /// 处理模式选择变化（自动/手动·常见/手动·正常/手动·困难）
    void onModeChanged(int index);

    /// 处理主题选择变化（浅色/深色/琥珀）
    void onThemeChanged(int index);

    /// 处理速度档位选择变化（慢速/中速/快速）
    void onSpeedChanged(int index);

private:
    // ==================== 常量 ====================

    /// 游戏绘图区域的像素宽度（GRID_SIZE * CELL_SIZE = 25 * 28 = 700）
    static const int CANVAS_WIDTH = 700;
    /// 游戏绘图区域的像素高度
    static const int CANVAS_HEIGHT = 700;

    // ==================== UI组件 ====================

    std::shared_ptr<SnakeGameModel> m_model;  ///< 游戏模型

    QPushButton* m_startBtn;    ///< 开始按钮
    QPushButton* m_pauseBtn;    ///< 暂停按钮
    QPushButton* m_resetBtn;    ///< 重置按钮

    QLabel* m_stateLabel;       ///< 状态标签
    QLabel* m_scoreLabel;       ///< 得分标签
    QLabel* m_eatenLabel;       ///< 已吃数量标签
    QLabel* m_targetLabel;      ///< 目标权重标签

    QComboBox* m_modeCombo;     ///< 模式选择下拉框（自动/手动·常见/手动·正常/手动·困难）
    QComboBox* m_themeCombo;    ///< 主题选择下拉框（浅色/深色/琥珀）
    QComboBox* m_speedCombo;    ///< 速度档位下拉框（慢速/中速/快速）

    QPushButton* m_leaderboardBtn;  ///< 排行榜按钮
    QPushButton* m_soundBtn;        ///< 音效开关按钮

    ViewTheme m_theme;          ///< 当前界面主题（QSettings 持久化）

    QVBoxLayout* m_mainLayout;  ///< 主布局
    QHBoxLayout* m_infoLayout;  ///< 得分信息行（画布上边界）
    QHBoxLayout* m_buttonLayout; ///< 控制按钮行（画布下边界）

    /// 当前 Model 模式对应的下拉框索引
    int currentModeComboIndex() const;

    // ==================== 绘制方法 ====================

    /// 绘制网格背景（棋盘格效果）
    void drawGrid(QPainter& painter);

    /// 绘制网格线
    void drawGridLines(QPainter& painter);

    /// 绘制蛇身（绿色渐变，头深尾浅）
    void drawSnake(QPainter& painter);

    /// 绘制食物（圆形，颜色根据权重映射，显示权重数字）
    void drawFoods(QPainter& painter);

    /// 绘制状态覆盖层（暂停遮罩 / AI让位横幅）
    /// @param painter 画家
    /// @param offsetX 画布在窗口中的水平偏移
    /// @param offsetY 画布在窗口中的垂直偏移
    void drawOverlay(QPainter& painter, int offsetX, int offsetY);

    /// 根据权重值计算颜色（低权重绿色→高权重红色）
    QColor getWeightColor(int weight) const;

    /// 更新所有状态标签的文字
    void updateLabels();

    /// 初始化UI控件和布局
    void setupUI();

    /// 应用主题：加载对应 QSS 文件（resources/ 多路径查找，失败回退内联样式）
    void applyTheme(ViewTheme theme);
};

#endif // SNAKEGAMEVIEW_H
