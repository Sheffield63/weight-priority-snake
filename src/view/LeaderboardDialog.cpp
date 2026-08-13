/**
 * @file LeaderboardDialog.cpp
 * @brief 排行榜弹窗实现
 *
 * 自包含样式（局部 QSS 覆盖祖先样式）：
 * - 大标题（🏆 排行榜 Top10）+ 浅灰分隔线
 * - 记录行：排名徽章 + 得分粗体 + 右侧用时/已吃/日期小字
 * - 前三名排名徽章金/银/铜色区分
 * - 深色主题与浅色主题两套配色（跟随主界面主题）
 */

#include "LeaderboardDialog.h"

#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>

LeaderboardDialog::LeaderboardDialog(const std::vector<LeaderboardEntry>& entries,
                                     ViewTheme theme, QWidget* parent)
    : QDialog(parent)
    , m_theme(theme)
{
    setModal(true);
    setFixedWidth(400);
    setWindowTitle(QString::fromUtf8("排行榜 🏆"));

    setStyleSheet(buildStyleSheet());

    // ---- Overall vertical layout ----
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 标题
    QLabel* title = new QLabel(QString::fromUtf8("🏆 排行榜 Top%1")
                                   .arg(LEADERBOARD_SIZE), this);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    QFrame* sep = new QFrame(this);
    sep->setObjectName("sepLine");
    sep->setFrameShape(QFrame::NoFrame);
    mainLayout->addWidget(sep);

    if (entries.empty()) {
        setupEmpty();
    } else {
        // 记录列表放入滚动区（最多10条通常无需滚动，但窗口较矮时兜底）
        QWidget* listHost = new QWidget(this);
        QVBoxLayout* listLayout = new QVBoxLayout(listHost);
        listLayout->setContentsMargins(24, 6, 24, 6);
        listLayout->setSpacing(6);

        const int count = static_cast<int>(entries.size());
        for (int i = 0; i < count; ++i) {
            addEntryRow(i + 1, entries[static_cast<size_t>(i)]);
        }

        QScrollArea* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setWidget(listHost);
        scroll->setFrameShape(QFrame::NoFrame);
        mainLayout->addWidget(scroll, 1);
    }

    // 底部关闭按钮
    QHBoxLayout* footLayout = new QHBoxLayout;
    footLayout->setContentsMargins(24, 12, 24, 20);
    QPushButton* closeBtn = new QPushButton(QString::fromUtf8("关 闭"), this);
    closeBtn->setObjectName("closeButton");
    closeBtn->setCursor(Qt::PointingHandCursor);
    footLayout->addStretch(1);
    footLayout->addWidget(closeBtn);
    footLayout->addStretch(1);
    mainLayout->addLayout(footLayout);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

QString LeaderboardDialog::buildStyleSheet() const {
    const bool dark = (m_theme == ViewTheme::DARK);
    return dark
        ? QString::fromUtf8(R"(
            QDialog {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #16202f,
                                            stop: 1 #0e1522);
            }
            QLabel#titleLabel {
                font-size: 22px;
                font-weight: bold;
                color: #ffcf6b;
                padding: 18px 24px 0 24px;
            }
            QFrame#sepLine {
                background: rgba(120, 150, 200, 0.25);
                border: none;
                min-height: 1px;
                max-height: 1px;
                margin: 10px 24px 4px 24px;
            }
            QFrame#entryRow {
                background: rgba(255, 255, 255, 0.06);
                border: 1px solid rgba(120, 150, 200, 0.18);
                border-radius: 10px;
            }
            QLabel#rankLabel {
                font-size: 15px;
                font-weight: bold;
                color: #8fa3c8;
                padding: 8px 0 8px 12px;
            }
            QLabel#scoreLabel {
                font-size: 16px;
                font-weight: bold;
                color: #eaf1fb;
                padding: 8px 0 8px 10px;
            }
            QLabel#metaLabel {
                font-size: 13px;
                color: #9fb2d4;
                padding: 8px 12px 8px 0;
            }
            QLabel#emptyLabel {
                font-size: 15px;
                color: #9fb2d4;
                padding: 36px 24px;
            }
            QPushButton#closeButton {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #2f80ed,
                                            stop: 1 #1f5fb8);
                color: #ffffff;
                border: 1px solid rgba(255, 255, 255, 50);
                border-radius: 12px;
                padding: 9px 26px;
                font-size: 15px;
                font-weight: bold;
                min-width: 100px;
            }
            QPushButton#closeButton:hover {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #4a95f5,
                                            stop: 1 #2a6fce);
            }
            QPushButton#closeButton:pressed {
                background: #1d57a8;
            }
            QScrollArea {
                background: transparent;
                border: none;
            }
        )")
        : QString::fromUtf8(R"(
            QDialog {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #ffffff,
                                            stop: 1 #eef4fa);
            }
            QLabel#titleLabel {
                font-size: 22px;
                font-weight: bold;
                color: #b45309;
                padding: 18px 24px 0 24px;
            }
            QFrame#sepLine {
                background: rgba(30, 60, 90, 0.12);
                border: none;
                min-height: 1px;
                max-height: 1px;
                margin: 10px 24px 4px 24px;
            }
            QFrame#entryRow {
                background: rgba(30, 60, 90, 0.06);
                border: 1px solid rgba(30, 60, 90, 0.14);
                border-radius: 10px;
            }
            QLabel#rankLabel {
                font-size: 15px;
                font-weight: bold;
                color: #5b6b7d;
                padding: 8px 0 8px 12px;
            }
            QLabel#scoreLabel {
                font-size: 16px;
                font-weight: bold;
                color: #24354a;
                padding: 8px 0 8px 10px;
            }
            QLabel#metaLabel {
                font-size: 13px;
                color: #7a8ba0;
                padding: 8px 12px 8px 0;
            }
            QLabel#emptyLabel {
                font-size: 15px;
                color: #7a8ba0;
                padding: 36px 24px;
            }
            QPushButton#closeButton {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #43e97b,
                                            stop: 1 #38a06c);
                color: #ffffff;
                border: 1px solid rgba(255, 255, 255, 60);
                border-radius: 12px;
                padding: 9px 26px;
                font-size: 15px;
                font-weight: bold;
                min-width: 100px;
            }
            QPushButton#closeButton:hover {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                            stop: 0 #5ff09a,
                                            stop: 1 #45b77d);
            }
            QPushButton#closeButton:pressed {
                background: #2c8055;
            }
            QScrollArea {
                background: transparent;
                border: none;
            }
        )");
}

void LeaderboardDialog::setupEmpty() {
    QLabel* empty = new QLabel(
        QString::fromUtf8("暂无记录\n完成一局游戏后自动生成"), this);
    empty->setObjectName("emptyLabel");
    empty->setAlignment(Qt::AlignCenter);
    layout()->addWidget(empty);
}

void LeaderboardDialog::addEntryRow(int rank, const LeaderboardEntry& entry) {
    QFrame* row = new QFrame(this);
    row->setObjectName("entryRow");

    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(0);

    // 排名徽章：前三名金/银/铜，其余灰色
    QLabel* rankLabel = new QLabel(QString::number(rank), row);
    rankLabel->setObjectName("rankLabel");
    if (rank == 1) {
        rankLabel->setStyleSheet(QString::fromUtf8("color: #d4a017;"));
    } else if (rank == 2) {
        rankLabel->setStyleSheet(QString::fromUtf8("color: #8d9aa8;"));
    } else if (rank == 3) {
        rankLabel->setStyleSheet(QString::fromUtf8("color: #b87333;"));
    }
    rankLabel->setMinimumWidth(36);
    rankLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowLayout->addWidget(rankLabel);

    // 得分（粗体醒目）
    QLabel* scoreLabel = new QLabel(
        QString::fromUtf8("%1 分").arg(entry.score), row);
    scoreLabel->setObjectName("scoreLabel");
    rowLayout->addWidget(scoreLabel);

    rowLayout->addStretch(1);

    // 右侧元信息：用时 · 已吃 · 日期
    const int minutes = entry.seconds / 60;
    const int remainSec = entry.seconds % 60;
    QLabel* metaLabel = new QLabel(
        QString::fromUtf8("%1分%2秒 · %3个 · %4")
            .arg(minutes)
            .arg(remainSec, 2, 10, QLatin1Char('0'))
            .arg(entry.eaten)
            .arg(entry.date.mid(5)),  // "MM-dd hh:mm"
        row);
    metaLabel->setObjectName("metaLabel");
    rowLayout->addWidget(metaLabel);

    layout()->addWidget(row);
}
