/**
 * @file SnakeGameController.cpp
 * @brief 贪吃蛇游戏控制器层实现
 *
 * 核心职责：
 * 1. 创建并连接Model和View
 * 2. 管理QTimer定时器（按速度档位驱动游戏tick，手动模式随成长动态加速）
 * 3. 根据游戏状态自动启停定时器
 * 4. 游戏结束时弹出结果消息框（含统计/新纪录/再来一局）
 * 5. 播放游戏音效（吃食/死亡/胜利）
 */

#include "SnakeGameController.h"
#include "src/model/SnakeGameModel.h"
#include "src/view/SnakeGameView.h"
#include "src/view/GameOverDialog.h"
#include "src/view/LeaderboardDialog.h"
#include <QTimer>
#include <QDialog>
#include <QSoundEffect>
#include <QUrl>
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QCoreApplication>

// ==================== 构造/析构 ====================

SnakeGameController::SnakeGameController(QObject* parent)
    : QObject(parent)
    , m_model(std::make_shared<SnakeGameModel>())
    , m_view(new SnakeGameView(m_model))
    , m_timer(new QTimer())
    , m_soundEat(new QSoundEffect())
    , m_soundDeath(new QSoundEffect())
    , m_soundWin(new QSoundEffect())
    , m_soundEnabled(true)
    , m_roundStartMs(0)
{
    // 从持久化存储加载音效开关（QSettings，键 settings/soundEnabled）
    {
        QSettings settings;
        m_soundEnabled = settings.value("settings/soundEnabled", true).toBool();
    }
    m_view->setSoundEnabled(m_soundEnabled);

    setupTimer();
    setupSounds();
    connectSignals();

    // 显示视图窗口
    m_view->show();
}

SnakeGameController::~SnakeGameController() = default;

// ==================== 内部初始化 ====================

void SnakeGameController::setupTimer() {
    // 基础间隔（按档位；手动模式由 onTimerTick 按成长动态调整）
    m_timer->setInterval(BASE_TICK_INTERVAL);

    // 连接定时器超时信号到游戏循环槽函数
    connect(m_timer.get(), &QTimer::timeout, this, &SnakeGameController::onTimerTick);
}

void SnakeGameController::setupSounds() {
    // 音效目录多路径查找：可执行文件旁 / 工作目录 / 源码树（构建与运行目录可能不同）
    const QStringList bases = {
        QCoreApplication::applicationDirPath() + "/resources/sounds",
        QDir::current().absoluteFilePath("resources/sounds"),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../resources/sounds")
    };
    QString soundDir;
    for (const QString& base : bases) {
        if (QDir(base).exists()) {
            soundDir = base;
            break;
        }
    }

    if (!soundDir.isEmpty()) {
        // 开发模式：从文件系统加载
        m_soundEat->setSource(QUrl::fromLocalFile(soundDir + "/eat.wav"));
        m_soundDeath->setSource(QUrl::fromLocalFile(soundDir + "/death.wav"));
        m_soundWin->setSource(QUrl::fromLocalFile(soundDir + "/win.wav"));
    } else {
        // 发布模式：音效已通过 resources/resources.qrc 编译进 exe，直接走 Qt 资源
        m_soundEat->setSource(QUrl(QStringLiteral("qrc:/sounds/eat.wav")));
        m_soundDeath->setSource(QUrl(QStringLiteral("qrc:/sounds/death.wav")));
        m_soundWin->setSource(QUrl(QStringLiteral("qrc:/sounds/win.wav")));
    }

    // 音量控制（避免突然刺耳）
    m_soundEat->setVolume(0.5);
    m_soundDeath->setVolume(0.6);
    m_soundWin->setVolume(0.5);
}

void SnakeGameController::connectSignals() {
    // 监听游戏状态变化，自动控制定时器启停
    connect(m_model.get(), &SnakeGameModel::gameStateChanged, this,
        [this](GameState state) {
            switch (state) {
                case GameState::PLAYING:
                    // 游戏开始/恢复 → 重置本局计时并启动定时器
                    m_roundStartMs = QDateTime::currentMSecsSinceEpoch();
                    m_timer->setInterval(m_model->getRecommendedTickInterval());
                    m_timer->start();
                    break;
                case GameState::PAUSED:
                case GameState::IDLE:
                case GameState::GAME_OVER:
                case GameState::WIN:
                    // 暂停/重置/结束 → 停止定时器
                    m_timer->stop();
                    break;
            }
        }
    );

    // 监听吃食信号 → 播放吃食音效
    connect(m_model.get(), &SnakeGameModel::foodEaten, this,
        [this]() { playEatSound(); });

    // 监听游戏结束信号，显示结算弹窗
    connect(m_model.get(), &SnakeGameModel::gameOver, this,
        [this](int finalScore) {
            // 本局统计
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const qint64 elapsedMs = (nowMs - m_roundStartMs) > 0
                ? (nowMs - m_roundStartMs) : 0;
            const int seconds = static_cast<int>(elapsedMs / 1000);
            const int minutes = seconds / 60;
            const int remainSec = seconds % 60;

            const bool isWin = (m_model->getGameState() == GameState::WIN);
            const bool newRecord = m_model->updateBestScore(finalScore);

            // 本局结果写入排行榜（Top10，QSettings 持久化）
            m_model->addLeaderboardEntry(finalScore, seconds,
                                         m_model->getEatenCount());

            // 播放结束音效
            if (isWin) {
                playWinSound();
            } else {
                playDeathSound();
            }

            // 失败原因（胜利时为空字符串）
            QString deathReason;
            if (!isWin) {
                deathReason = (m_model->getControlMode() == ControlMode::AUTO)
                    ? QString::fromUtf8("蛇已无路可走。")
                    : QString::fromUtf8("蛇撞墙或撞到自己，或触犯了难度规则。");
            }

            // 结算弹窗：再来一局 / 关闭（配色跟随主界面主题）
            GameOverDialog dlg(isWin, deathReason, finalScore,
                               m_model->getEatenCount(),
                               m_model->getTotalFoodCount(),
                               minutes, remainSec,
                               m_model->getBestScore(),
                               newRecord,
                               m_view->getTheme(), m_view.get());
            if (dlg.exec() == QDialog::Accepted) {
                // 重置并直接开始新一局
                m_model->reset();
                m_model->start();
            }
        }
    );

    // ESC 快捷键：暂停/恢复（Model 内部仅在 PLAYING/PAUSED 响应）
    connect(m_view.get(), &SnakeGameView::escapePressed,
            this, &SnakeGameController::onEscapePressed);

    // 音效开关按钮
    connect(m_view.get(), &SnakeGameView::soundToggleRequested,
            this, &SnakeGameController::onSoundToggle);

    // 排行榜按钮：弹出排行榜弹窗
    connect(m_view.get(), &SnakeGameView::leaderboardRequested,
            this, &SnakeGameController::onLeaderboardRequested);
}

// ==================== 槽函数 ====================

void SnakeGameController::onTimerTick() {
    // 如果游戏正在运行，执行一步游戏逻辑
    if (m_model->getGameState() == GameState::PLAYING) {
        m_model->tick();

        // 动态速度：手动模式随成长加速，自动模式恒定按档位（getRecommendedTickInterval 内部区分）
        const int recommended = m_model->getRecommendedTickInterval();
        if (m_timer->interval() != recommended) {
            m_timer->setInterval(recommended);
        }
    }
}

void SnakeGameController::onEscapePressed() {
    // ESC 暂停/恢复（Model 内部仅在 PLAYING/PAUSED 状态响应，其余状态无副作用）
    m_model->togglePause();
}

void SnakeGameController::onSoundToggle() {
    m_soundEnabled = !m_soundEnabled;
    // 持久化音效开关（下次启动恢复）
    QSettings settings;
    settings.setValue("settings/soundEnabled", m_soundEnabled);
    // 同步按钮文案
    m_view->setSoundEnabled(m_soundEnabled);
}

void SnakeGameController::onLeaderboardRequested() {
    // 弹出排行榜弹窗（配色跟随主界面主题）
    LeaderboardDialog dlg(m_model->getLeaderboard(),
                          m_view->getTheme(), m_view.get());
    dlg.exec();
}

// ==================== 音效 ====================

void SnakeGameController::playEatSound() {
    if (m_soundEnabled && m_soundEat->isLoaded()) {
        m_soundEat->play();
    }
}

void SnakeGameController::playDeathSound() {
    if (m_soundEnabled && m_soundDeath->isLoaded()) {
        m_soundDeath->play();
    }
}

void SnakeGameController::playWinSound() {
    if (m_soundEnabled && m_soundWin->isLoaded()) {
        m_soundWin->play();
    }
}
