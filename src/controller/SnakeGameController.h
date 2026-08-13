/**
 * @file SnakeGameController.h
 * @brief 贪吃蛇游戏控制器层
 *
 * Controller是Model和View之间的桥梁。
 * 主要职责：管理QTimer驱动游戏循环（动态速度），处理游戏结束弹窗，
 * 播放游戏音效（吃食/死亡/胜利）。
 * UI按钮交互由View直接调用Model方法。
 */

#ifndef SNAKEGAMECONTROLLER_H
#define SNAKEGAMECONTROLLER_H

#include <QObject>
#include <memory>

// 前向声明
class SnakeGameModel;
class SnakeGameView;
class QTimer;
class QSoundEffect;

/**
 * @brief 游戏控制器类
 *
 * 管理游戏循环定时器（QTimer，按速度档位设定基准间隔，手动模式随成长加速），
 * 监听模型状态变化来控制定时器启停，
 * 在游戏结束时显示结果弹窗（含统计与"再来一局"）。
 */
class SnakeGameController : public QObject {
    Q_OBJECT

public:
    explicit SnakeGameController(QObject* parent = nullptr);
    ~SnakeGameController();

    SnakeGameView* getView() const { return m_view.get(); }

private slots:
    void onTimerTick();

    /// ESC 快捷键：暂停/恢复（仅 PLAYING/PAUSED 状态生效）
    void onEscapePressed();

    /// 点击音效开关按钮：切换音效启用状态（QSettings 持久化）
    void onSoundToggle();

    /// 点击排行榜按钮：弹出排行榜弹窗
    void onLeaderboardRequested();

private:
    std::shared_ptr<SnakeGameModel> m_model;
    std::unique_ptr<SnakeGameView> m_view;
    std::unique_ptr<QTimer> m_timer;

    // 游戏音效（QSoundEffect，WAV 资源）
    std::unique_ptr<QSoundEffect> m_soundEat;
    std::unique_ptr<QSoundEffect> m_soundDeath;
    std::unique_ptr<QSoundEffect> m_soundWin;

    // 音效总开关（QSettings 持久化，关闭时静音所有反馈音）
    bool m_soundEnabled;

    // 本局计时（从 PLAYING 开始到结束）
    qint64 m_roundStartMs;

    void setupTimer();
    void connectSignals();
    void setupSounds();

    /// 播放吃食音效（受音效开关控制）
    void playEatSound();
    /// 播放死亡音效（受音效开关控制）
    void playDeathSound();
    /// 播放胜利音效（受音效开关控制）
    void playWinSound();
};

#endif // SNAKEGAMECONTROLLER_H
