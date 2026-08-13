/**
 * @file LeaderboardDialog.h
 * @brief 排行榜弹窗（Top10）
 *
 * 展示历史最佳成绩列表：排名 + 得分 + 用时 + 已吃 + 日期。
 * 主题跟随主界面（浅色/深色/琥珀统一走浅色或深色两套内联 QSS）。
 */

#ifndef LEADERBOARDDIALOG_H
#define LEADERBOARDDIALOG_H

#include <QDialog>
#include <vector>

#include "src/model/SnakeGameModel.h"  // LeaderboardEntry
#include "src/view/Theme.h"            // ViewTheme

class QLabel;

class LeaderboardDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief 构造排行榜弹窗
     * @param entries 排行榜记录（按得分降序，最多 LEADERBOARD_SIZE 条）
     * @param theme   当前界面主题（决定弹窗配色）
     * @param parent  父窗口（主视图）
     */
    explicit LeaderboardDialog(const std::vector<LeaderboardEntry>& entries,
                               ViewTheme theme,
                               QWidget* parent = nullptr);

private:
    ViewTheme m_theme;  ///< 主题（决定深色/浅色两套内联 QSS）

    /// 组装内联 QSS（浅色/深色两套）
    QString buildStyleSheet() const;

    /// 无记录时的空态提示
    void setupEmpty();

    /// 组装单行记录：排名 + 得分 + 用时/已吃/日期
    void addEntryRow(int rank, const LeaderboardEntry& entry);
};

#endif // LEADERBOARDDIALOG_H
