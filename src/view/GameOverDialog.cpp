/**
 * @file GameOverDialog.cpp
 * @brief 游戏结算弹窗实现
 *
 * 卡片风格，自包含样式（局部 QSS 覆盖祖先样式），配色跟随主界面主题：
 * - 大标题（胜利绿 / 失败红）+ 失败原因小字
 * - 浅灰分隔线
 * - 统计网格：标签右对齐、数值左对齐（两列对齐整洁）
 * - 金色"新纪录"横幅（仅破纪录时显示）
 * - 主次按钮：主色渐变"再来一局" / 次色"关闭"
 */

#include "GameOverDialog.h"

#include <QLabel>
#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

GameOverDialog::GameOverDialog(bool isWin, const QString& reason,
                               int score, int eaten, int totalFood,
                               int minutes, int seconds,
                               int bestScore, bool newRecord,
                               ViewTheme theme, QWidget* parent)
    : QDialog(parent)
    , m_theme(theme)
{
    // Modal + fixed width keeps the card layout clean on every platform
    setModal(true);
    setFixedWidth(400);
    setWindowTitle(isWin ? QString::fromUtf8("胜利 🎉")
                         : QString::fromUtf8("游戏结束 💀"));

    // Self-contained stylesheet (beats ancestor styles), colors follow theme
    setStyleSheet(buildStyleSheet());

    // ---- Overall vertical layout ----
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupTitle(isWin, reason);
    setupStats(score, eaten, totalFood, minutes, seconds, bestScore);
    setupRecordBanner(newRecord);
    setupButtons();
}

QString GameOverDialog::buildStyleSheet() const {
    const bool dark = (m_theme == ViewTheme::DARK);
    return dark
        ? QString::fromUtf8(R"(
            QDialog {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #16202f,
                                            stop: 1 #0e1522);
            }
            QLabel#titleLabel {
                font-size: 26px;
                font-weight: bold;
                padding: 22px 24px 0 24px;
            }
            QLabel#reasonLabel {
                font-size: 14px;
                color: #9fb2d4;
                padding: 4px 24px 0 24px;
            }
            QFrame#sepLine {
                background: rgba(120, 150, 200, 0.25);
                border: none;
                min-height: 1px;
                max-height: 1px;
                margin: 14px 24px 8px 24px;
            }
            QLabel#statLabel {
                font-size: 15px;
                color: #9fb2d4;
                padding: 6px 0;
            }
            QLabel#statValue {
                font-size: 17px;
                font-weight: bold;
                color: #eaf1fb;
                padding: 6px 0;
            }
            QFrame#recordBanner {
                background: rgba(255, 190, 60, 0.12);
                border: 1px solid rgba(255, 190, 60, 0.35);
                border-radius: 10px;
                margin: 10px 24px 6px 24px;
            }
            QLabel#recordLabel {
                font-size: 17px;
                font-weight: bold;
                color: #ffcf6b;
                padding: 7px 12px;
            }
            QPushButton#retryButton {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #2f80ed,
                                            stop: 1 #1f5fb8);
                color: #ffffff;
                border: 1px solid rgba(255, 255, 255, 50);
                border-radius: 12px;
                padding: 10px 26px;
                font-size: 16px;
                font-weight: bold;
                min-width: 130px;
            }
            QPushButton#retryButton:hover {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #4a95f5,
                                            stop: 1 #2a6fce);
            }
            QPushButton#retryButton:pressed {
                background: #1d57a8;
            }
            QPushButton#closeButton {
                background: rgba(255, 255, 255, 0.08);
                color: #c8d6ee;
                border: 1px solid rgba(120, 150, 200, 0.25);
                border-radius: 12px;
                padding: 10px 22px;
                font-size: 16px;
                font-weight: 600;
                min-width: 90px;
            }
            QPushButton#closeButton:hover {
                background: rgba(255, 255, 255, 0.14);
            }
            QPushButton#closeButton:pressed {
                background: rgba(255, 255, 255, 0.2);
            }
        )")
        : QString::fromUtf8(R"(
            QDialog {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #ffffff,
                                            stop: 1 #eef4fa);
            }
            QLabel#titleLabel {
                font-size: 26px;
                font-weight: bold;
                padding: 22px 24px 0 24px;
            }
            QLabel#reasonLabel {
                font-size: 14px;
                color: #7a8ba0;
                padding: 4px 24px 0 24px;
            }
            QFrame#sepLine {
                background: rgba(30, 60, 90, 0.12);
                border: none;
                min-height: 1px;
                max-height: 1px;
                margin: 14px 24px 8px 24px;
            }
            QLabel#statLabel {
                font-size: 15px;
                color: #5b6b7d;
                padding: 6px 0;
            }
            QLabel#statValue {
                font-size: 17px;
                font-weight: bold;
                color: #24354a;
                padding: 6px 0;
            }
            QFrame#recordBanner {
                background: rgba(255, 190, 60, 0.18);
                border: 1px solid rgba(190, 130, 10, 0.35);
                border-radius: 10px;
                margin: 10px 24px 6px 24px;
            }
            QLabel#recordLabel {
                font-size: 17px;
                font-weight: bold;
                color: #b45309;
                padding: 7px 12px;
            }
            QPushButton#retryButton {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #43e97b,
                                            stop: 1 #38a06c);
                color: #ffffff;
                border: 1px solid rgba(255, 255, 255, 60);
                border-radius: 12px;
                padding: 10px 26px;
                font-size: 16px;
                font-weight: bold;
                min-width: 130px;
            }
            QPushButton#retryButton:hover {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #5ff09a,
                                            stop: 1 #45b77d);
            }
            QPushButton#retryButton:pressed {
                background: #2c8055;
            }
            QPushButton#closeButton {
                background: rgba(200, 210, 225, 0.35);
                color: #4a5a6d;
                border: 1px solid rgba(30, 60, 90, 0.18);
                border-radius: 12px;
                padding: 10px 22px;
                font-size: 16px;
                font-weight: 600;
                min-width: 90px;
            }
            QPushButton#closeButton:hover {
                background: rgba(185, 198, 215, 0.5);
            }
            QPushButton#closeButton:pressed {
                background: rgba(170, 185, 205, 0.6);
            }
        )");
}

void GameOverDialog::setupTitle(bool isWin, const QString& reason) {
    QLabel* title = new QLabel(
        isWin ? QString::fromUtf8("🎉 恭喜你赢了！")
              : QString::fromUtf8("💀 游戏结束"), this);
    title->setObjectName("titleLabel");
    // Victory green / defeat red overrides the neutral title style;
    // darker theme uses brighter tones for contrast
    const bool dark = (m_theme == ViewTheme::DARK);
    title->setStyleSheet(isWin
        ? (dark ? QString::fromUtf8("color: #3ddc84;")
                : QString::fromUtf8("color: #1f9d55;"))
        : (dark ? QString::fromUtf8("color: #ff6b6b;")
                : QString::fromUtf8("color: #c0392b;")));

    layout()->addWidget(title);

    if (!isWin && !reason.isEmpty()) {
        QLabel* reasonLabel = new QLabel(reason, this);
        reasonLabel->setObjectName("reasonLabel");
        layout()->addWidget(reasonLabel);
    }
}

void GameOverDialog::setupStats(int score, int eaten, int totalFood,
                                int minutes, int seconds, int bestScore) {
    QFrame* sep = new QFrame(this);
    sep->setObjectName("sepLine");
    sep->setFrameShape(QFrame::NoFrame);
    layout()->addWidget(sep);

    QGridLayout* grid = new QGridLayout;
    grid->setContentsMargins(24, 4, 24, 4);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(2);

    const struct { QString label; QString value; } rows[] = {
        { QString::fromUtf8("总得分"), QString::number(score) },
        { QString::fromUtf8("已吃零食"),
          QString::fromUtf8("%1/%2").arg(eaten).arg(totalFood) },
        { QString::fromUtf8("本局用时"),
          QString::fromUtf8("%1分%2秒")
              .arg(minutes).arg(seconds, 2, 10, QLatin1Char('0')) },
        { QString::fromUtf8("历史最高"), QString::number(bestScore) }
    };

    const int rowCount = static_cast<int>(sizeof(rows) / sizeof(rows[0]));
    for (int i = 0; i < rowCount; ++i) {
        QLabel* label = new QLabel(rows[i].label, this);
        label->setObjectName("statLabel");
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        QLabel* value = new QLabel(rows[i].value, this);
        value->setObjectName("statValue");
        value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        grid->addWidget(label, i, 0);
        grid->addWidget(value, i, 1);
    }

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    layout()->addItem(grid);
}

void GameOverDialog::setupRecordBanner(bool newRecord) {
    if (!newRecord) {
        return;
    }

    QFrame* banner = new QFrame(this);
    banner->setObjectName("recordBanner");

    QHBoxLayout* bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* recordLabel = new QLabel(QString::fromUtf8("🏆 新纪录！"), banner);
    recordLabel->setObjectName("recordLabel");
    recordLabel->setAlignment(Qt::AlignCenter);

    bannerLayout->addWidget(recordLabel);
    layout()->addWidget(banner);
}

void GameOverDialog::setupButtons() {
    QWidget* buttonRow = new QWidget(this);
    QHBoxLayout* rowLayout = new QHBoxLayout(buttonRow);
    rowLayout->setContentsMargins(24, 12, 24, 20);
    rowLayout->setSpacing(16);

    QPushButton* retryButton = new QPushButton(
        QString::fromUtf8("🔄 再来一局"), buttonRow);
    retryButton->setObjectName("retryButton");
    retryButton->setCursor(Qt::PointingHandCursor);

    QPushButton* closeButton = new QPushButton(
        QString::fromUtf8("关闭"), buttonRow);
    closeButton->setObjectName("closeButton");
    closeButton->setCursor(Qt::PointingHandCursor);

    rowLayout->addStretch(1);
    rowLayout->addWidget(retryButton);
    rowLayout->addWidget(closeButton);
    rowLayout->addStretch(1);

    layout()->addWidget(buttonRow);

    connect(retryButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}
