/**
 * @file SnakeGameView.cpp
 * @brief 贪吃蛇游戏视图层实现
 *
 * 绘制细节：
 * - 网格背景：浅灰色棋盘格效果
 * - 蛇身：绿色渐变（头部深绿→尾部浅绿），蛇头画白色眼睛
 * - 食物：圆形，颜色从绿(低权重)到红(高权重)渐变，显示权重数字
 * - 状态信息：得分、已吃数量、当前目标权重、游戏状态
 * - 控制按钮：开始、暂停、重置
 */

#include "SnakeGameView.h"
#include "src/model/SnakeGameModel.h"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QFrame>
#include <QFile>
#include <QShortcut>
#include <QKeySequence>
#include <QSettings>

// ==================== 构造/析构 ====================

SnakeGameView::SnakeGameView(std::shared_ptr<SnakeGameModel> model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_startBtn(nullptr)
    , m_pauseBtn(nullptr)
    , m_resetBtn(nullptr)
    , m_stateLabel(nullptr)
    , m_scoreLabel(nullptr)
    , m_eatenLabel(nullptr)
    , m_targetLabel(nullptr)
    , m_modeCombo(nullptr)
    , m_themeCombo(nullptr)
    , m_speedCombo(nullptr)
    , m_leaderboardBtn(nullptr)
    , m_soundBtn(nullptr)
    , m_theme(ViewTheme::LIGHT)
    , m_mainLayout(nullptr)
{
    // 从持久化存储加载界面主题（QSettings，键 settings/theme）
    {
        QSettings settings;
        m_theme = themeFromPersistValue(
            settings.value("settings/theme", themePersistValue(ViewTheme::LIGHT)).toInt());
    }

    setupUI();

    // 连接模型信号到视图更新
    connect(m_model.get(), &SnakeGameModel::gameDataUpdated, this, [this]() {
        updateLabels();
        update(); // 触发重绘
    });

    connect(m_model.get(), &SnakeGameModel::gameStateChanged, this, [this](GameState /*state*/) {
        updateLabels();
        update();
    });

    // 方向键 + WASD 快捷键（手动模式控制蛇移动；自动模式下由 Model 内部忽略）
    // 使用 QShortcut 而非 keyPressEvent，避免焦点落在下拉框/按钮上时按键失效
    struct ShortcutSpec {
        QKeySequence sequence;
        int dx;
        int dy;
    };
    const ShortcutSpec shortcuts[] = {
        { QKeySequence(Qt::Key_Up),    0, -1 },
        { QKeySequence(Qt::Key_Down),  0,  1 },
        { QKeySequence(Qt::Key_Left), -1,  0 },
        { QKeySequence(Qt::Key_Right), 1,  0 },
        { QKeySequence(Qt::Key_W),     0, -1 },
        { QKeySequence(Qt::Key_S),     0,  1 },
        { QKeySequence(Qt::Key_A),    -1,  0 },
        { QKeySequence(Qt::Key_D),     1,  0 }
    };
    for (const ShortcutSpec& spec : shortcuts) {
        QShortcut* shortcut = new QShortcut(spec.sequence, this);
        connect(shortcut, &QShortcut::activated, this, [this, spec]() {
            m_model->setMoveDirection(spec.dx, spec.dy);
        });
    }

    // ESC 快捷键：暂停/恢复（任何焦点下均生效，发射信号由 Controller 处理）
    {
        QShortcut* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        escShortcut->setContext(Qt::WindowShortcut);
        connect(escShortcut, &QShortcut::activated, this, &SnakeGameView::escapePressed);
    }

    // 初始标签状态
    updateLabels();
}

SnakeGameView::~SnakeGameView() = default;

// ==================== UI初始化 ====================

void SnakeGameView::setupUI() {
    // 设置窗口标题
    setWindowTitle(QString::fromUtf8("权重优先贪吃蛇"));
    // 设置 objectName：QSS 中的 QWidget#SnakeGameView 背景选择器依赖此名匹配
    setObjectName("SnakeGameView");

    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(8);

    // ===== 模式选择行（置顶，左对齐不拉伸） =====
    QHBoxLayout* modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(8);

    QLabel* modeCaption = new QLabel(QString::fromUtf8("模式"), this);
    modeCaption->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    modeLayout->addWidget(modeCaption);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QString::fromUtf8("自动（权重AI）"));
    m_modeCombo->addItem(QString::fromUtf8("手动 · 常见"));
    m_modeCombo->addItem(QString::fromUtf8("手动 · 正常"));
    m_modeCombo->addItem(QString::fromUtf8("手动 · 困难"));
    m_modeCombo->setToolTip(QString::fromUtf8("自动: A*寻路自动吃食\n手动: 方向键/WASD控制\n常见: 零食无限刷新，铺满地图获胜\n正常: 大小零食均可吃，吃完20个获胜\n困难: 场上仍有更小零食时吃大零食判负"));
    modeLayout->addWidget(m_modeCombo);  // 固定宽度，不拉伸
    modeLayout->addStretch();            // 剩余空间推到右侧，combo 靠左

    m_mainLayout->addLayout(modeLayout);

    // ===== 偏好选择行（主题 + 速度档位，左对齐不拉伸） =====
    QHBoxLayout* prefLayout = new QHBoxLayout();
    prefLayout->setSpacing(8);

    QLabel* themeCaption = new QLabel(QString::fromUtf8("主题"), this);
    themeCaption->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    prefLayout->addWidget(themeCaption);

    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem(QString::fromUtf8("浅色"));
    m_themeCombo->addItem(QString::fromUtf8("深色"));
    m_themeCombo->addItem(QString::fromUtf8("琥珀"));
    m_themeCombo->setToolTip(QString::fromUtf8("界面配色主题（即时生效并记忆）"));
    m_themeCombo->setCurrentIndex(themePersistValue(m_theme));
    prefLayout->addWidget(m_themeCombo);

    prefLayout->addSpacing(10);

    QLabel* speedCaption = new QLabel(QString::fromUtf8("速度"), this);
    speedCaption->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    prefLayout->addWidget(speedCaption);

    m_speedCombo = new QComboBox(this);
    m_speedCombo->addItem(QString::fromUtf8("慢速 · 220ms"));
    m_speedCombo->addItem(QString::fromUtf8("中速 · 150ms"));
    m_speedCombo->addItem(QString::fromUtf8("快速 · 90ms"));
    m_speedCombo->setToolTip(QString::fromUtf8("自动/手动模式均生效；手动模式还会随吃食进一步加速"));
    m_speedCombo->setCurrentIndex(static_cast<int>(m_model->getSpeedLevel()));
    prefLayout->addWidget(m_speedCombo);

    prefLayout->addStretch();

    m_mainLayout->addLayout(prefLayout);

    // ===== 状态信息行 =====
    m_stateLabel = new QLabel(QString::fromUtf8("状态: 未开始"), this);
    m_stateLabel->setObjectName("stateLabel");
    m_stateLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_stateLabel);

    // ===== 得分信息卡片行（深色玻璃卡，视觉分组） =====
    QFrame* infoCard = new QFrame(this);
    infoCard->setObjectName("infoCard");
    m_infoLayout = new QHBoxLayout(infoCard);
    m_infoLayout->setContentsMargins(10, 4, 10, 4);
    m_infoLayout->setSpacing(4);

    m_scoreLabel = new QLabel(QString::fromUtf8("得分: 0"), infoCard);
    m_scoreLabel->setObjectName("scoreLabel");
    m_scoreLabel->setAlignment(Qt::AlignCenter);
    m_infoLayout->addWidget(m_scoreLabel, 1);  // 等宽分布

    m_eatenLabel = new QLabel(QString::fromUtf8("已吃: 0/20"), infoCard);
    m_eatenLabel->setObjectName("eatenLabel");
    m_eatenLabel->setAlignment(Qt::AlignCenter);
    m_infoLayout->addWidget(m_eatenLabel, 1);

    m_targetLabel = new QLabel(QString::fromUtf8("目标: -"), infoCard);
    m_targetLabel->setObjectName("targetLabel");
    m_targetLabel->setAlignment(Qt::AlignCenter);
    m_infoLayout->addWidget(m_targetLabel, 1);

    m_mainLayout->addWidget(infoCard);

    // 弹性空间：将按钮行推到窗口底部，画布（paintEvent绘制）居中于两者之间
    m_mainLayout->addStretch(1);

    // ===== 绘图区域占位（通过paintEvent绘制） =====
    // 竖屏布局：窗口固定 720×1280，画布 700×700 在顶部控件与按钮行之间垂直居中
    setFixedSize(720, 1280);
    // 实际绘图区域在paintEvent中定位

    // ===== 控制按钮行 =====
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(18);

    m_startBtn = new QPushButton(QString::fromUtf8("开 始"), this);
    m_pauseBtn = new QPushButton(QString::fromUtf8("暂 停"), this);
    m_resetBtn = new QPushButton(QString::fromUtf8("重 置"), this);
    m_leaderboardBtn = new QPushButton(QString::fromUtf8("排行榜"), this);
    m_soundBtn = new QPushButton(QString::fromUtf8("音效: 开"), this);

    m_buttonLayout->addWidget(m_startBtn, 1);  // 等宽均匀分布
    m_buttonLayout->addWidget(m_pauseBtn, 1);
    m_buttonLayout->addWidget(m_resetBtn, 1);
    m_buttonLayout->addWidget(m_leaderboardBtn, 1);
    m_buttonLayout->addWidget(m_soundBtn, 1);

    m_mainLayout->addLayout(m_buttonLayout);

    // ===== 连接按钮信号到槽函数 =====
    connect(m_startBtn, &QPushButton::clicked, this, &SnakeGameView::onStartClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &SnakeGameView::onPauseClicked);
    connect(m_resetBtn, &QPushButton::clicked, this, &SnakeGameView::onResetClicked);
    connect(m_leaderboardBtn, &QPushButton::clicked, this, &SnakeGameView::leaderboardRequested);
    connect(m_soundBtn, &QPushButton::clicked, this, &SnakeGameView::soundToggleRequested);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SnakeGameView::onModeChanged);
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SnakeGameView::onThemeChanged);
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SnakeGameView::onSpeedChanged);

    // ===== 加载主题样式 =====
    applyTheme(m_theme);
}

// ==================== 事件处理 ====================

void SnakeGameView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘图区域偏移：画布在顶部控件（信息卡片下方）与底部按钮行之间垂直居中
    int offsetX = (width() - CANVAS_WIDTH) / 2;
    if (offsetX < 0) offsetX = 0;
    int top = (m_infoLayout && m_infoLayout->parentWidget() &&
               m_infoLayout->parentWidget()->geometry().height() > 0)
        ? m_infoLayout->parentWidget()->geometry().bottom() + m_mainLayout->spacing()
        : 100;
    int bottom = (m_buttonLayout && m_buttonLayout->geometry().height() > 0)
        ? m_buttonLayout->geometry().top() - m_mainLayout->spacing()
        : height() - 50;
    int availableH = bottom - top;
    int offsetY = top + (availableH - CANVAS_HEIGHT) / 2;
    if (offsetY < top) offsetY = top;

    // 保存绘图状态，设置裁剪区域
    painter.save();
    painter.translate(offsetX, offsetY);

    // 绘制网格背景
    drawGrid(painter);

    // 绘制网格线
    drawGridLines(painter);

    // 绘制食物
    drawFoods(painter);

    // 绘制蛇身
    drawSnake(painter);

    painter.restore();

    // 画布圆角边框（玻璃质感描边，颜色随主题适配）
    const QColor borderColor = (m_theme == ViewTheme::DARK)
        ? QColor(120, 150, 200, 90)
        : QColor(60, 90, 130, 50);
    painter.setPen(QPen(borderColor, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(offsetX, offsetY, CANVAS_WIDTH, CANVAS_HEIGHT, 10, 10);

    // 状态覆盖层（暂停遮罩 / AI让位横幅）
    drawOverlay(painter, offsetX, offsetY);
}

// ==================== 槽函数 ====================

void SnakeGameView::onStartClicked() {
    m_model->start();
}

void SnakeGameView::onPauseClicked() {
    m_model->togglePause();
}

void SnakeGameView::onResetClicked() {
    m_model->reset();
}

void SnakeGameView::onModeChanged(int index) {
    // 游戏中禁止切换模式（Model 内部同样校验 IDLE，此处同步回滚下拉框显示）
    if (m_model->getGameState() != GameState::IDLE) {
        QSignalBlocker blocker(m_modeCombo);
        m_modeCombo->setCurrentIndex(currentModeComboIndex());
        return;
    }

    switch (index) {
        case 1:
            m_model->setControlMode(ControlMode::MANUAL);
            m_model->setGameMode(GameMode::CLASSIC);
            break;
        case 2:
            m_model->setControlMode(ControlMode::MANUAL);
            m_model->setGameMode(GameMode::NORMAL);
            break;
        case 3:
            m_model->setControlMode(ControlMode::MANUAL);
            m_model->setGameMode(GameMode::HARD);
            break;
        case 0:
        default:
            m_model->setControlMode(ControlMode::AUTO);
            break;
    }
    updateLabels();
}

int SnakeGameView::currentModeComboIndex() const {
    if (m_model->getControlMode() == ControlMode::AUTO) {
        return 0;
    }
    switch (m_model->getGameMode()) {
        case GameMode::CLASSIC: return 1;
        case GameMode::NORMAL:  return 2;
        case GameMode::HARD:    return 3;
    }
    return 0;
}

// ==================== 主题与偏好 ====================

void SnakeGameView::setTheme(ViewTheme theme) {
    if (m_theme == theme) {
        return;
    }
    m_theme = theme;
    QSignalBlocker blocker(m_themeCombo);
    m_themeCombo->setCurrentIndex(themePersistValue(theme));
    applyTheme(theme);
    // 持久化主题选择（下次启动恢复）
    QSettings settings;
    settings.setValue("settings/theme", themePersistValue(theme));
    update();  // 画布边框颜色随主题变化，立即重绘
}

void SnakeGameView::applyTheme(ViewTheme theme) {
    // 尝试从多个路径加载对应主题的QSS文件
    const QString fileName = themeFileName(theme);
    QStringList possiblePaths = {
        "resources/" + fileName,
        "../resources/" + fileName,
        "../../resources/" + fileName,
        ":/resources/" + fileName
    };

    bool styleLoaded = false;
    for (const QString& path : possiblePaths) {
        QFile styleFile(path);
        if (styleFile.open(QFile::ReadOnly)) {
            QString styleSheet = QLatin1String(styleFile.readAll());
            setStyleSheet(styleSheet);
            styleFile.close();
            styleLoaded = true;
            break;
        }
    }

    // 如果QSS文件未找到，使用内联兜底样式
    if (!styleLoaded) {
        setStyleSheet(
            "QPushButton {"
            "    background-color: #4CAF50;"
            "    color: white;"
            "    border: none;"
            "    padding: 8px 20px;"
            "    font-size: 14px;"
            "    font-weight: bold;"
            "    border-radius: 4px;"
            "    min-width: 80px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #45a049;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #3d8b40;"
            "}"
            "QPushButton:disabled {"
            "    background-color: #cccccc;"
            "    color: #666666;"
            "}"
            "QLabel {"
            "    font-size: 14px;"
            "    color: #333333;"
            "    padding: 4px;"
            "}"
        );
    }
}

void SnakeGameView::setSoundEnabled(bool enabled) {
    m_soundBtn->setText(enabled ? QString::fromUtf8("音效: 开")
                                : QString::fromUtf8("音效: 关"));
}

void SnakeGameView::onThemeChanged(int index) {
    // 切换主题：应用对应 QSS 并持久化（IDLE/游戏中均可即时切换）
    setTheme(themeFromPersistValue(index));
}

void SnakeGameView::onSpeedChanged(int index) {
    // 速度档位（慢速/中速/快速），由 Model 持久化
    if (index >= static_cast<int>(SpeedLevel::SLOW) &&
        index <= static_cast<int>(SpeedLevel::FAST)) {
        m_model->setSpeedLevel(static_cast<SpeedLevel>(index));
    }
}

// ==================== 绘制方法 ====================

void SnakeGameView::drawGrid(QPainter& painter) {
    // 棋盘格主色随主题适配（深色主题下保持低亮棋盘，避免刺眼）
    const QColor cellA = (m_theme == ViewTheme::DARK) ? QColor(24, 30, 42)
        : (m_theme == ViewTheme::AMBER) ? QColor(245, 237, 222)
        : QColor(240, 240, 240);
    const QColor cellB = (m_theme == ViewTheme::DARK) ? QColor(29, 36, 50)
        : (m_theme == ViewTheme::AMBER) ? QColor(251, 244, 232)
        : QColor(250, 250, 250);

    // 绘制棋盘格效果的网格背景
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            // 交替使用两种浅色
            QColor bgColor = ((x + y) % 2 == 0) ? cellA : cellB;

            painter.setPen(Qt::NoPen);
            painter.setBrush(bgColor);
            painter.drawRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
        }
    }
}

void SnakeGameView::drawGridLines(QPainter& painter) {
    // 网格线颜色随主题适配
    const QColor lineColor = (m_theme == ViewTheme::DARK) ? QColor(60, 74, 100)
        : (m_theme == ViewTheme::AMBER) ? QColor(214, 196, 172)
        : QColor(200, 200, 200);

    // 绘制浅灰色网格线
    QPen gridPen(lineColor, 1);
    painter.setPen(gridPen);
    painter.setBrush(Qt::NoBrush);

    // 绘制垂直线
    for (int x = 0; x <= GRID_SIZE; ++x) {
        painter.drawLine(x * CELL_SIZE, 0, x * CELL_SIZE, GRID_SIZE * CELL_SIZE);
    }

    // 绘制水平线
    for (int y = 0; y <= GRID_SIZE; ++y) {
        painter.drawLine(0, y * CELL_SIZE, GRID_SIZE * CELL_SIZE, y * CELL_SIZE);
    }
}

void SnakeGameView::drawSnake(QPainter& painter) {
    const auto& snake = m_model->getSnake();
    if (snake.empty()) {
        return;
    }

    int totalSegments = static_cast<int>(snake.size());

    // 蛇头发光效果（在蛇身下方绘制光晕）
    {
        const Position& head = snake.front();
        QPoint headCenter(head.x * CELL_SIZE + CELL_SIZE / 2,
                          head.y * CELL_SIZE + CELL_SIZE / 2);
        QRadialGradient headGlow(QPointF(headCenter), CELL_SIZE * 2.0);
        headGlow.setColorAt(0.0, QColor(80, 220, 120, 110));
        headGlow.setColorAt(1.0, QColor(80, 220, 120, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(headGlow);
        painter.drawEllipse(QPointF(headCenter), CELL_SIZE * 2.0, CELL_SIZE * 2.0);
    }

    for (int i = 0; i < totalSegments; ++i) {
        const Position& pos = snake[i];

        // 计算渐变颜色：头部深绿(0,150,0) → 尾部浅绿(144,238,144)
        float ratio = (totalSegments > 1) ?
            static_cast<float>(i) / static_cast<float>(totalSegments - 1) : 0.0f;

        int r = static_cast<int>(0 + ratio * 144);
        int g = static_cast<int>(150 + ratio * (238 - 150));
        int b = static_cast<int>(0 + ratio * 144);

        QColor snakeColor(r, g, b);
        painter.setPen(Qt::NoPen);
        painter.setBrush(snakeColor);

        // 蛇身格子内缩2像素，留出网格间隙效果
        int padding = 2;
        int drawX = pos.x * CELL_SIZE + padding;
        int drawY = pos.y * CELL_SIZE + padding;
        int drawSize = CELL_SIZE - 2 * padding;

        painter.drawRoundedRect(drawX, drawY, drawSize, drawSize, 4, 4);

        // 蛇头画两个白色眼睛
        if (i == 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::white);

            int eyeSize = 6;
            int eyeOffsetX1 = pos.x * CELL_SIZE + 7;
            int eyeOffsetX2 = pos.x * CELL_SIZE + 15;
            int eyeOffsetY = pos.y * CELL_SIZE + 7;

            painter.drawEllipse(eyeOffsetX1, eyeOffsetY, eyeSize, eyeSize);
            painter.drawEllipse(eyeOffsetX2, eyeOffsetY, eyeSize, eyeSize);

            // 瞳孔
            painter.setBrush(Qt::black);
            int pupilSize = 3;
            painter.drawEllipse(eyeOffsetX1 + 1, eyeOffsetY + 1, pupilSize, pupilSize);
            painter.drawEllipse(eyeOffsetX2 + 1, eyeOffsetY + 1, pupilSize, pupilSize);
        }
    }
}

void SnakeGameView::drawFoods(QPainter& painter) {
    const auto& foods = m_model->getFoods();

    for (const auto& food : foods) {
        if (food.eaten) {
            continue; // 跳过已吃掉的食物
        }

        int centerX = food.position.x * CELL_SIZE + CELL_SIZE / 2;
        int centerY = food.position.y * CELL_SIZE + CELL_SIZE / 2;
        int radius = 10;

        // 获取权重对应的颜色
        QColor foodColor = getWeightColor(food.weight);

        // 食物发光效果（在食物下方绘制光晕）
        {
            QRadialGradient foodGlow(QPointF(centerX, centerY), radius * 2.5);
            foodGlow.setColorAt(0.0, QColor(foodColor.red(), foodColor.green(), foodColor.blue(), 90));
            foodGlow.setColorAt(1.0, QColor(foodColor.red(), foodColor.green(), foodColor.blue(), 0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(foodGlow);
            painter.drawEllipse(QPointF(centerX, centerY), radius * 2.5, radius * 2.5);
        }

        // 绘制食物圆形
        painter.setPen(QPen(QColor(80, 80, 80), 1)); // 深灰色边框
        painter.setBrush(foodColor);
        painter.drawEllipse(QPoint(centerX, centerY), radius, radius);

        // 绘制权重数字
        // 根据背景亮度决定文字颜色（深色背景用白字，浅色背景用黑字）
        int brightness = (foodColor.red() * 299 + foodColor.green() * 587 + foodColor.blue() * 114) / 1000;
        QColor textColor = (brightness < 128) ? Qt::white : Qt::black;

        painter.setPen(textColor);
        QFont weightFont = painter.font();
        weightFont.setPointSize(11);
        weightFont.setBold(true);
        painter.setFont(weightFont);

        QString weightStr = QString::number(food.weight);
        QRect textRect(centerX - 8, centerY - 8, 16, 16);
        painter.drawText(textRect, Qt::AlignCenter, weightStr);
    }
}

QColor SnakeGameView::getWeightColor(int weight) const {
    // 权重范围大约1-8（正态分布均值3，标准差1.5）
    // 映射到色相：权重1 → 色相120°(绿色) → 权重8 → 色相0°(红色)
    int maxExpectedWeight = 8;
    int minWeight = 1;

    // 将权重归一化到[0, 1]范围
    float normalized = static_cast<float>(weight - minWeight) /
                       static_cast<float>(maxExpectedWeight - minWeight);
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    // HSV色相：从120°(绿色)到0°(红色)
    int hue = static_cast<int>(120.0f * (1.0f - normalized));

    return QColor::fromHsv(hue, 200, 220);
}

void SnakeGameView::drawOverlay(QPainter& painter, int offsetX, int offsetY) {
    const GameState state = m_model->getGameState();

    // AI 让位横幅：仅在游戏中且 AI 正在避让时显示
    if (state == GameState::PLAYING && m_model->isEvading()) {
        const int bannerH = 40;
        QRect banner(offsetX, offsetY, CANVAS_WIDTH, bannerH);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 152, 0, 210)); // 橙色半透明
        painter.drawRoundedRect(banner, 8, 8);

        painter.setPen(Qt::white);
        QFont bannerFont = painter.font();
        bannerFont.setPointSize(12);
        bannerFont.setBold(true);
        painter.setFont(bannerFont);
        painter.drawText(banner, Qt::AlignCenter, QString::fromUtf8("⚠ AI 正在让位避让…"));
    }

    // 暂停遮罩：半透明黑色 + 中央提示
    if (state == GameState::PAUSED) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 130));
        painter.drawRoundedRect(offsetX, offsetY, CANVAS_WIDTH, CANVAS_HEIGHT, 10, 10);

        painter.setPen(Qt::white);
        QFont pauseFont = painter.font();
        pauseFont.setPointSize(26);
        pauseFont.setBold(true);
        painter.setFont(pauseFont);
        painter.drawText(QRect(offsetX, offsetY, CANVAS_WIDTH, CANVAS_HEIGHT),
                         Qt::AlignCenter, QString::fromUtf8("⏸ 已暂停"));
    }
}

void SnakeGameView::updateLabels() {
    // 更新状态标签
    QString stateText;
    switch (m_model->getGameState()) {
        case GameState::IDLE:
            stateText = QString::fromUtf8("状态: 未开始");
            break;
        case GameState::PLAYING:
            stateText = QString::fromUtf8("状态: 游戏中");
            break;
        case GameState::PAUSED:
            stateText = QString::fromUtf8("状态: 已暂停");
            break;
        case GameState::GAME_OVER:
            stateText = QString::fromUtf8("状态: 游戏结束");
            break;
        case GameState::WIN:
            stateText = QString::fromUtf8("状态: 胜利！");
            break;
    }
    m_stateLabel->setText(stateText);

    // 模式下拉框：仅 IDLE 状态可切换（游戏中禁止中途变更规则）
    m_modeCombo->setEnabled(m_model->getGameState() == GameState::IDLE);

    // 更新得分标签
    m_scoreLabel->setText(QString::fromUtf8("得分: %1").arg(m_model->getScore()));

    // 更新已吃数量标签
    m_eatenLabel->setText(
        QString::fromUtf8("已吃: %1/%2").arg(m_model->getEatenCount()).arg(m_model->getTotalFoodCount())
    );

    // 更新目标权重标签
    int targetWeight = m_model->getCurrentTargetWeight();
    if (targetWeight > 0) {
        m_targetLabel->setText(QString::fromUtf8("目标: %1").arg(targetWeight));
    } else {
        m_targetLabel->setText(QString::fromUtf8("目标: 无"));
    }
}
