/**
 * @file GameOverDialog.h
 * @brief 游戏结算弹窗（胜利 / 失败）
 *
 * 浅色卡片风格：大标题 + 死亡原因小字 + 分隔线 + 对齐统计网格
 * + 新纪录横幅 + 主次按钮。
 * exec() 返回 QDialog::Accepted 表示用户点击"再来一局"。
 */

#ifndef GAMEOVERDIALOG_H
#define GAMEOVERDIALOG_H

#include <QDialog>

#include "src/view/Theme.h"  // ViewTheme

class QLabel;
class QFrame;

class GameOverDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief 构造结算弹窗
     * @param isWin      是否胜利（决定标题文案与颜色）
     * @param reason     失败原因说明（胜利时为空）
     * @param score      本局得分
     * @param eaten      已吃零食数
     * @param totalFood  零食总数
     * @param minutes    本局用时（分）
     * @param seconds    本局用时（秒）
     * @param bestScore  历史最高分
     * @param newRecord  是否刷新历史纪录（决定是否显示金色横幅）
     * @param theme      当前界面主题（决定弹窗深色/浅色配色）
     * @param parent     父窗口（主视图）
     */
    explicit GameOverDialog(bool isWin,
                            const QString& reason,
                            int score,
                            int eaten,
                            int totalFood,
                            int minutes,
                            int seconds,
                            int bestScore,
                            bool newRecord,
                            ViewTheme theme,
                            QWidget* parent = nullptr);

private:
    ViewTheme m_theme;  ///< 主题（决定深色/浅色两套内联 QSS）

    QString buildStyleSheet() const;

    void setupTitle(bool isWin, const QString& reason);
    void setupStats(int score, int eaten, int totalFood,
                    int minutes, int seconds, int bestScore);
    void setupRecordBanner(bool newRecord);
    void setupButtons();
};

#endif // GAMEOVERDIALOG_H
