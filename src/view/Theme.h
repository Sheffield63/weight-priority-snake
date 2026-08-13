/**
 * @file Theme.h
 * @brief 界面主题枚举（View / GameOverDialog / LeaderboardDialog 共用）
 *
 * 与 resources/ 下的 QSS 文件一一对应：
 * LIGHT -> styles.qss（浅色玻璃）
 * DARK  -> styles_dark.qss（深色玻璃）
 * AMBER -> styles_amber.qss（琥珀暖调）
 */

#ifndef THEME_H
#define THEME_H

#include <QString>

/**
 * @brief 界面主题
 */
enum class ViewTheme {
    LIGHT,  ///< 浅色（默认）
    DARK,   ///< 深色
    AMBER   ///< 琥珀暖调
};

/// 主题显示名（下拉框文案）
inline QString themeDisplayName(ViewTheme theme) {
    switch (theme) {
        case ViewTheme::LIGHT: return QString::fromUtf8("浅色");
        case ViewTheme::DARK:  return QString::fromUtf8("深色");
        case ViewTheme::AMBER: return QString::fromUtf8("琥珀");
    }
    return QString();
}

/// 主题对应的 QSS 资源文件名（不含路径）
inline QString themeFileName(ViewTheme theme) {
    switch (theme) {
        case ViewTheme::LIGHT: return QStringLiteral("styles.qss");
        case ViewTheme::DARK:  return QStringLiteral("styles_dark.qss");
        case ViewTheme::AMBER: return QStringLiteral("styles_amber.qss");
    }
    return QString();
}

/// QSettings 持久化键值（0/1/2）
inline int themePersistValue(ViewTheme theme) {
    return static_cast<int>(theme);
}

/// 从持久化整数值还原主题（非法值回退浅色）
inline ViewTheme themeFromPersistValue(int value) {
    if (value >= static_cast<int>(ViewTheme::LIGHT) &&
        value <= static_cast<int>(ViewTheme::AMBER)) {
        return static_cast<ViewTheme>(value);
    }
    return ViewTheme::LIGHT;
}

#endif // THEME_H
