/**
 * @file SnakeGameModel.cpp
 * @brief 贪吃蛇游戏模型层实现
 *
 * 核心逻辑包括：
 * 1. 蛇身移动与增长（growthCounter机制）
 * 2. A*寻路集成（每吃到零食后重新计算路径）
 * 3. 食物生成与管理（正态分布权重+随机位置）
 * 4. 碰撞检测（自身碰撞判断）
 * 5. 游戏状态机管理
 */

#include "SnakeGameModel.h"
#include "src/algorithm/AStar.h"
#include <QSettings>
#include <QDateTime>
#include <QStringList>
#include <QRandomGenerator>
#include <cmath>
#include <climits>
#include <cstring>
#include <numeric>
#include <algorithm>

// ==================== 构造函数 ====================

SnakeGameModel::SnakeGameModel(QObject* parent)
    : QObject(parent)
    , m_state(GameState::IDLE)
    , m_score(0)
    , m_eatenCount(0)
    , m_totalFoodSpawned(FOOD_COUNT)
    , m_bestScore(0)
    , m_currentTargetIndex(-1)
    , m_pathIndex(0)
    , m_growthCounter(0)
    , m_evading(false)
    , m_fixedStartLayout(false)
    , m_gameMode(GameMode::NORMAL)
    , m_controlMode(ControlMode::AUTO)
    , m_dirX(1)
    , m_dirY(0)
    , m_speedLevel(SpeedLevel::NORMAL)
    , m_posDist(0, GRID_SIZE - 1)
{
    // 占用网格清零（625B，一次 memset 覆盖全部 cache line）
    std::memset(m_occupancy, 0, sizeof(m_occupancy));

    // 使用系统熵初始化随机数种子：
    // 注意：std::random_device 在 MinGW-w64 上已知会退化为固定序列
    //（同一进程/每次启动生成相同地图，造成"玩过"的观感），
    // 改用 Qt QRandomGenerator::system()（Windows 上基于 BCrypt 系统熵，
    // 每次启动真随机，跨进程不重复）。
    m_rng.seed(QRandomGenerator::system()->generate());

    // 从持久化存储加载历史最高分与速度档位
    QSettings settings;
    m_bestScore = settings.value("game/bestScore", 0).toInt();
    const int speedLevel = settings.value("settings/speedLevel",
                                          static_cast<int>(SpeedLevel::NORMAL)).toInt();
    if (speedLevel >= static_cast<int>(SpeedLevel::SLOW) &&
        speedLevel <= static_cast<int>(SpeedLevel::FAST)) {
        m_speedLevel = static_cast<SpeedLevel>(speedLevel);
    }

    // 初始化游戏
    reset();
}

// ==================== 公共接口 ====================

void SnakeGameModel::reset() {
    // 清空所有状态数据
    m_snake.clear();
    m_foods.clear();
    m_state = GameState::IDLE;
    m_score = 0;
    m_eatenCount = 0;
    m_totalFoodSpawned = FOOD_COUNT;
    m_currentTargetIndex = -1;
    m_currentPath.clear();
    m_pathIndex = 0;
    m_growthCounter = 0;
    m_evading = false;
    m_dirX = 1;  // 手动模式默认向右
    m_dirY = 0;

    // 占用网格清零，与 m_snake.clear() 保持同步
    std::memset(m_occupancy, 0, sizeof(m_occupancy));

    // 初始化蛇和食物
    initSnake();
    generateFoods();

    // 通知视图更新
    emit gameStateChanged(m_state);
    emit gameDataUpdated();
}

void SnakeGameModel::start() {
    // 只有在IDLE状态下才能开始游戏
    if (m_state != GameState::IDLE) {
        return;
    }

    m_state = GameState::PLAYING;

    if (m_controlMode == ControlMode::AUTO) {
        // 自动模式：选择第一个目标并计算路径
        selectNextTarget();
        computePath();
    }
    // 手动模式：等待玩家方向键输入（默认向右），无需 A* 规划

    emit gameStateChanged(m_state);
}

void SnakeGameModel::togglePause() {
    if (m_state == GameState::PLAYING) {
        m_state = GameState::PAUSED;
    } else if (m_state == GameState::PAUSED) {
        m_state = GameState::PLAYING;
    } else {
        return; // 其他状态不响应暂停操作
    }

    emit gameStateChanged(m_state);
}

void SnakeGameModel::tick() {
    // 只有PLAYING状态才执行游戏逻辑
    if (m_state != GameState::PLAYING) {
        return;
    }

    // 按操作方式分派：自动 AI（A*寻路 + 让位绕行）/ 手动方向键
    if (m_controlMode == ControlMode::MANUAL) {
        manualTick();
    } else {
        autoTick();
    }
}

void SnakeGameModel::setGameMode(GameMode mode) {
    // 仅 IDLE 状态允许切换，避免游戏中中途改变规则
    if (m_state != GameState::IDLE) {
        return;
    }
    m_gameMode = mode;
}

void SnakeGameModel::setControlMode(ControlMode mode) {
    if (m_state != GameState::IDLE) {
        return;
    }
    m_controlMode = mode;
}

void SnakeGameModel::setMoveDirection(int dx, int dy) {
    // 仅手动模式生效
    if (m_controlMode != ControlMode::MANUAL) {
        return;
    }
    if (dx == 0 && dy == 0) {
        return;
    }
    // 禁止 180° 直接掉头（防止瞬间反向撞到自身）
    if (m_dirX == -dx && m_dirY == -dy) {
        return;
    }
    m_dirX = dx;
    m_dirY = dy;
}

void SnakeGameModel::autoTick() {
    // 只有PLAYING状态才执行游戏逻辑
    if (m_state != GameState::PLAYING) {
        return;
    }

    // ===== 让位模式（Issue 2）：每 tick 重试目标路径，通道让出即切回正常模式 =====
    if (m_evading) {
        if (tryComputePathToTarget(m_currentTargetIndex)) {
            m_evading = false; // 通道已腾出，恢复寻路
        } else {
            // 执行一步安全绕行（兜圈子/折叠自身，腾出通向目标的通道）
            if (!evadeStep()) {
                // 所有方向均不安全：真正死局
                m_state = GameState::GAME_OVER;
                emit gameStateChanged(m_state);
                emit gameOver(m_score);
                return;
            }
            emit gameDataUpdated();
            return; // 本 tick 仅绕行一步，下个 tick 继续重试
        }
    }

    // ===== 正常模式 =====
    // 如果当前没有路径或路径已走完，需要重新计算
    if (m_currentPath.empty() || m_pathIndex >= m_currentPath.size()) {
        // 优先重算当前目标（严格模式：避开非目标零食的纯空白路径）
        if (!tryComputePathToTarget(m_currentTargetIndex)) {
            // 当前目标不可达：尝试选择其他可达目标
            bool foundAlternative = false;
            for (size_t i = 0; i < m_foods.size(); ++i) {
                if (m_foods[i].eaten) continue;
                m_currentTargetIndex = static_cast<int>(i);
                if (tryComputePathToTarget(static_cast<int>(i))) {
                    foundAlternative = true;
                    break;
                }
            }

            if (!foundAlternative) {
                // 所有目标均不可达：进入让位模式，绕行腾出通道
                // （例如蛇身很长时形成自墙，需要折叠自身让出通向最后零食的路）
                m_evading = true;
                if (!evadeStep()) {
                    // 连绕行空间都没有：真正死局
                    m_state = GameState::GAME_OVER;
                    emit gameStateChanged(m_state);
                    emit gameOver(m_score);
                    return;
                }
                emit gameDataUpdated();
                return;
            }
        }
    }

    // 执行一步移动
    moveStep();

    // 检查食物碰撞
    checkFoodCollision();

    // 通知视图更新
    emit gameDataUpdated();
}

int SnakeGameModel::getCurrentTargetWeight() const {
    if (m_controlMode == ControlMode::MANUAL) {
        // 手动模式：无 AI 目标，返回场上最小未吃零食权重（供 UI 显示"最小目标"参考）
        int minWeight = -1;
        for (const auto& food : m_foods) {
            if (!food.eaten && (minWeight < 0 || food.weight < minWeight)) {
                minWeight = food.weight;
            }
        }
        return (minWeight < 0) ? 0 : minWeight;
    }

    if (m_currentTargetIndex >= 0 &&
        m_currentTargetIndex < static_cast<int>(m_foods.size()) &&
        !m_foods[m_currentTargetIndex].eaten)
    {
        return m_foods[m_currentTargetIndex].weight;
    }
    return 0;
}

bool SnakeGameModel::isOccupied(int x, int y) const {
    // O(1) 占用网格查表：625B 常驻 L1，单次字节读取。
    // 原实现为 O(n) 线性扫描 deque，被 generateFoods 调用 625 次/tick。
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) {
        return false;
    }
    return m_occupancy[y * GRID_SIZE + x] != 0;
}

int SnakeGameModel::getRecommendedTickInterval() const {
    // 基准间隔由所选速度档位决定（自动/手动模式均生效）
    int base = BASE_TICK_INTERVAL;
    switch (m_speedLevel) {
        case SpeedLevel::SLOW:   base = 220; break;  // 慢速：明显放慢
        case SpeedLevel::NORMAL: base = 150; break;  // 中速（默认）
        case SpeedLevel::FAST:   base = 90;  break;  // 快速：明显加快
    }

    // 自动模式：恒定按档位基准运行（AI 节奏稳定，不随成长变化）
    if (m_controlMode == ControlMode::AUTO) {
        return base;
    }

    // 手动模式：以所选档位为基准，每吃 10 个零食提速一档（每档快 15ms），下限 MIN_TICK_INTERVAL
    int interval = base - (m_eatenCount / 10) * 15;
    if (interval < MIN_TICK_INTERVAL) {
        interval = MIN_TICK_INTERVAL;
    }
    return interval;
}

void SnakeGameModel::setSpeedLevel(SpeedLevel level) {
    if (m_speedLevel == level) {
        return;
    }
    m_speedLevel = level;
    QSettings settings;
    settings.setValue("settings/speedLevel", static_cast<int>(level));
}

// ==================== 排行榜（Top10） ====================

std::vector<LeaderboardEntry> SnakeGameModel::getLeaderboard() const {
    std::vector<LeaderboardEntry> entries;
    QSettings settings;
    const QStringList list = settings.value("leaderboard/entries").toStringList();
    for (const QString& item : list) {
        const QStringList parts = item.split('|');
        if (parts.size() != 4) {
            continue;
        }
        bool okScore = false, okSeconds = false, okEaten = false;
        LeaderboardEntry entry;
        entry.score = parts[0].toInt(&okScore);
        entry.seconds = parts[1].toInt(&okSeconds);
        entry.eaten = parts[2].toInt(&okEaten);
        entry.date = parts[3];
        if (okScore && okSeconds && okEaten) {
            entries.push_back(entry);
        }
    }
    return entries;
}

bool SnakeGameModel::addLeaderboardEntry(int score, int seconds, int eaten) {
    LeaderboardEntry entry;
    entry.score = score;
    entry.seconds = seconds;
    entry.eaten = eaten;
    entry.date = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");

    std::vector<LeaderboardEntry> entries = getLeaderboard();
    entries.push_back(entry);

    // 按得分降序；同分时用时更短者靠前（稳定排序保证新记录在同分组末位）
    std::stable_sort(entries.begin(), entries.end(),
        [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
            if (a.score != b.score) {
                return a.score > b.score;
            }
            return a.seconds < b.seconds;
        });

    // 判定新记录是否入榜（截断前定位）
    const auto it = std::find(entries.begin(), entries.end(), entry);
    const bool inBoard = (it != entries.end()) &&
        (static_cast<size_t>(std::distance(entries.begin(), it)) < LEADERBOARD_SIZE);

    // 截断 Top10 并写回
    if (entries.size() > static_cast<size_t>(LEADERBOARD_SIZE)) {
        entries.resize(LEADERBOARD_SIZE);
    }
    QStringList list;
    for (const LeaderboardEntry& e : entries) {
        list << QString("%1|%2|%3|%4")
                    .arg(e.score).arg(e.seconds).arg(e.eaten).arg(e.date);
    }
    QSettings settings;
    settings.setValue("leaderboard/entries", list);

    return inBoard;
}

bool SnakeGameModel::updateBestScore(int score) {
    if (score <= m_bestScore) {
        return false;
    }
    m_bestScore = score;
    QSettings settings;
    settings.setValue("game/bestScore", m_bestScore);
    return true;
}

// ==================== 内部方法 ====================

void SnakeGameModel::setFixedStartLayout() {
    // 固定确定性开局布局（测试专用）：设置后立即重建布局，
    // initSnake 使用蛇头居中、长度 1、方向向右的历史语义，且跨多次 reset 持续生效。
    m_fixedStartLayout = true;
    reset();
}

void SnakeGameModel::initSnake() {
    m_snake.clear();

    if (m_fixedStartLayout) {
        // 固定测试布局：蛇头居中、长度 1（方向由 reset 保持向右）
        Position head;
        head.x = GRID_SIZE / 2;  // 12
        head.y = GRID_SIZE / 2;  // 12
        m_snake.push_front(head);
        // 同步占用网格（蛇头占据中央格）
        m_occupancy[head.y * GRID_SIZE + head.x] = 1;
        return;
    }

    // 随机开局布局：每次 reset 生成全新地图（位置/方向/长度各不相同）。
    // 蛇头坐标限制在 [2, GRID_SIZE-3] 区间：避开边缘 2 格，
    // 同时保证长度为 3 的蛇身沿任意方向伸展都不越界。
    const int minCoord = 2;
    const int maxCoord = GRID_SIZE - 3;  // 22
    std::uniform_int_distribution<int> coordDist(minCoord, maxCoord);
    std::uniform_int_distribution<int> dirDist(0, 3);

    // 四向随机初始方向（手动模式的方向键基准，自动模式由 A* 决定）
    int dx = 0;
    int dy = 0;
    switch (dirDist(m_rng)) {
        case 0: dx = 1;  dy = 0; break;  // right
        case 1: dx = -1; dy = 0; break;  // left
        case 2: dx = 0;  dy = -1; break; // up
        default: dx = 0; dy = 1; break;  // down
    }
    m_dirX = dx;
    m_dirY = dy;

    // 蛇身沿初始方向的反方向排布（头在最前，身体依次后延）
    Position head;
    head.x = coordDist(m_rng);
    head.y = coordDist(m_rng);
    for (int i = 0; i < RANDOM_SNAKE_LENGTH; ++i) {
        Position seg;
        seg.x = head.x - dx * i;
        seg.y = head.y - dy * i;
        m_snake.push_back(seg);
        // 同步占用网格
        m_occupancy[seg.y * GRID_SIZE + seg.x] = 1;
    }
}

void SnakeGameModel::generateFoods() {
    // 第一步：生成权重值
    std::vector<int> weights = generateWeights();

    // 第二步：随机放置食物到空位置
    // 收集所有空闲位置
    std::vector<Position> emptyPositions;

    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            // 跳过蛇占据的位置
            if (isOccupied(x, y)) {
                continue;
            }
            emptyPositions.push_back({x, y});
        }
    }

    // 随机打乱空闲位置
    std::shuffle(emptyPositions.begin(), emptyPositions.end(), m_rng);

    // 从前FOOD_COUNT个位置中放置食物
    for (int i = 0; i < FOOD_COUNT && i < static_cast<int>(emptyPositions.size()); ++i) {
        FoodItem food;
        food.position = emptyPositions[i];
        food.weight = weights[i];
        food.eaten = false;
        m_foods.push_back(food);
    }
}

void SnakeGameModel::buildObstacleFlat(uint8_t out[GRID_SIZE * GRID_SIZE]) const {
    // 从占用网格一次性线性拷贝（625B 顺序读 → 硬件预取器高效填充）。
    // 原实现为嵌套 vector<vector<bool>>：25 次堆分配 + 位压缩读写。
    std::memcpy(out, m_occupancy, sizeof(m_occupancy));

    // 蛇头可通行：清除蛇头占据位
    if (!m_snake.empty()) {
        const Position& head = m_snake.front();
        out[head.y * GRID_SIZE + head.x] = 0;
    }
}

void SnakeGameModel::selectNextTarget() {
    m_currentTargetIndex = -1;

    // 第一遍：全场最小未吃权重（目标主规则，与 HARD 难度语义保持一致）
    int minWeight = INT_MAX;
    for (size_t i = 0; i < m_foods.size(); ++i) {
        if (m_foods[i].eaten) {
            continue;
        }
        minWeight = std::min(minWeight, m_foods[i].weight);
    }
    if (minWeight == INT_MAX) {
        return;  // 无未吃食物
    }

    // 第二遍：次小权重（吃掉最小权重后的下一目标权重）
    int nextWeight = INT_MAX;
    for (size_t i = 0; i < m_foods.size(); ++i) {
        if (m_foods[i].eaten || m_foods[i].weight <= minWeight) {
            continue;
        }
        nextWeight = std::min(nextWeight, m_foods[i].weight);
    }

    // 第三遍：在最小权重候选中做前瞻选择。
    // 评分 = 蛇头到候选的距离 + 候选到"次小权重中最近零食"的距离。
    // 后者使吃掉当前目标后，蛇头天然靠近下一目标，缩短衔接路径。
    // 无次小权重（全场权重相同）时退化为原行为：取最近候选。
    Position snakeHead = m_snake.front();
    int bestScore = INT_MAX;

    for (size_t i = 0; i < m_foods.size(); ++i) {
        if (m_foods[i].eaten || m_foods[i].weight != minWeight) {
            continue;
        }

        int score = manhattanDistance(snakeHead, m_foods[i].position);

        if (nextWeight != INT_MAX) {
            int followUp = INT_MAX;
            for (size_t j = 0; j < m_foods.size(); ++j) {
                if (m_foods[j].eaten || m_foods[j].weight != nextWeight) {
                    continue;
                }
                followUp = std::min(followUp,
                    manhattanDistance(m_foods[i].position, m_foods[j].position));
            }
            score += followUp;  // followUp 必然有限（存在次小权重零食）
        }

        if (score < bestScore) {
            bestScore = score;
            m_currentTargetIndex = static_cast<int>(i);
        }
    }
}

void SnakeGameModel::computePath() {
    // 统一走 strict-first 两级寻路（Issue 1）
    tryComputePathToTarget(m_currentTargetIndex);
}

void SnakeGameModel::buildObstacleFlatForTarget(int foodIndex, uint8_t out[GRID_SIZE * GRID_SIZE]) const {
    // 基线：蛇身占位（顺序 memcpy，625B 线性拷贝）
    std::memcpy(out, m_occupancy, sizeof(m_occupancy));

    // 蛇头可通行：清除蛇头占据位
    if (!m_snake.empty()) {
        const Position& head = m_snake.front();
        out[head.y * GRID_SIZE + head.x] = 0;
    }

    // Issue 1：除目标外的所有未吃零食也标记为障碍。
    // A* 于是只规划经过"空白路径点"的最优路线，绝不顺路经过非目标零食。
    for (size_t i = 0; i < m_foods.size(); ++i) {
        if (m_foods[i].eaten || static_cast<int>(i) == foodIndex) {
            continue;
        }
        out[m_foods[i].position.y * GRID_SIZE + m_foods[i].position.x] = 1;
    }
}

bool SnakeGameModel::tryComputePathToTarget(int foodIndex) {
    if (foodIndex < 0 ||
        foodIndex >= static_cast<int>(m_foods.size()) ||
        m_foods[foodIndex].eaten)
    {
        m_currentPath.clear();
        m_pathIndex = 0;
        return false;
    }

    Position head = m_snake.front();
    const FoodItem& target = m_foods[foodIndex];
    uint8_t obstacleFlat[GRID_SIZE * GRID_SIZE];

    // 第一级：严格模式。非目标零食为障碍，只走空白路径点。
    buildObstacleFlatForTarget(foodIndex, obstacleFlat);
    m_currentPath = AStar::findPath(
        head.x, head.y,
        target.position.x, target.position.y,
        obstacleFlat,
        GRID_SIZE
    );
    if (!m_currentPath.empty()) {
        m_pathIndex = 0;
        // 路径含起点（蛇头）时跳过
        if (m_currentPath[0].first == head.x && m_currentPath[0].second == head.y) {
            m_pathIndex = 1;
        }
        return true;
    }

    // 第二级：宽松模式兜底（严格路径不存在，如目标被其他零食围死）。
    // 此时允许经过非目标零食，保证游戏可推进而非死锁。
    buildObstacleFlat(obstacleFlat);
    m_currentPath = AStar::findPath(
        head.x, head.y,
        target.position.x, target.position.y,
        obstacleFlat,
        GRID_SIZE
    );
    m_pathIndex = 0;
    if (!m_currentPath.empty()) {
        if (m_currentPath[0].first == head.x && m_currentPath[0].second == head.y) {
            m_pathIndex = 1;
        }
        return true;
    }

    return false;
}

void SnakeGameModel::moveStep() {
    // 检查路径是否有效
    if (m_currentPath.empty() || m_pathIndex >= m_currentPath.size()) {
        return;
    }

    // 获取路径中的下一个位置作为新蛇头
    Position newHead;
    newHead.x = m_currentPath[m_pathIndex].first;
    newHead.y = m_currentPath[m_pathIndex].second;
    m_pathIndex++;

    // 判断新蛇头位置是否有食物（决定是否保留蛇尾，即蛇身增长）
    bool ateFood = false;
    for (const FoodItem& food : m_foods) {
        if (!food.eaten && food.position.x == newHead.x && food.position.y == newHead.y) {
            ateFood = true;
            break;
        }
    }

    applyMove(newHead, ateFood);
}

void SnakeGameModel::applyMove(const Position& newHead, bool ateFood) {
    // 在deque前端插入新蛇头
    m_snake.push_front(newHead);

    // 同步占用网格：新蛇头置位
    m_occupancy[newHead.y * GRID_SIZE + newHead.x] = 1;

    // 吃到食物：保留蛇尾（蛇身增长），增长计数由 checkFoodCollision 累计。
    // 未吃到：按增长计数器决定是否移除尾部。
    if (ateFood) {
        return;
    }
    if (m_growthCounter > 0) {
        // 还有增长余量，不移除尾部
        m_growthCounter--;
        return;
    }
    // 正常移除尾部并同步清除占用位
    Position tail = m_snake.back();
    m_snake.pop_back();
    m_occupancy[tail.y * GRID_SIZE + tail.x] = 0;
}

bool SnakeGameModel::isEvadeMoveSafe(const Position& newHead, bool tailFrees) const {
    // 边界检查
    if (newHead.x < 0 || newHead.x >= GRID_SIZE || newHead.y < 0 || newHead.y >= GRID_SIZE) {
        return false;
    }
    const int headIdx = newHead.y * GRID_SIZE + newHead.x;

    // 蛇身碰撞：允许追入"即将腾出的蛇尾"格（此时该格移动后变为空地）
    const Position& tail = m_snake.back();
    if (m_occupancy[headIdx] != 0) {
        if (!(tailFrees && newHead.x == tail.x && newHead.y == tail.y)) {
            return false;
        }
    }

    // 绕行期间避开所有未吃零食（不顺路吃，等通道打开后走正规路径去吃）
    for (const FoodItem& f : m_foods) {
        if (!f.eaten && f.position.x == newHead.x && f.position.y == newHead.y) {
            return false;
        }
    }

    // ===== flood-fill 安全判据 =====
    // 模拟移动后的蛇身布局：尾腾出则旧尾格变空；新头格被占用。
    // 障碍 = 模拟蛇身 + 所有未吃零食。
    // 从新头 flood-fill 统计可达自由格数，须 >= 移动后蛇身长度：
    // 蛇有足够空间弯曲（兜圈子）而不撞自己 → 该移动安全。
    uint8_t blocked[GRID_SIZE * GRID_SIZE];
    std::memcpy(blocked, m_occupancy, sizeof(m_occupancy));
    if (tailFrees) {
        blocked[tail.y * GRID_SIZE + tail.x] = 0;
    }
    for (const FoodItem& f : m_foods) {
        if (!f.eaten) {
            blocked[f.position.y * GRID_SIZE + f.position.x] = 1;
        }
    }
    blocked[headIdx] = 1; // 新蛇头占据其格

    const int snakeLenAfter = static_cast<int>(m_snake.size()) + (tailFrees ? 0 : 1);

    // BFS flood-fill（固定大小数组，零堆分配；625B visited 常驻 L1）
    uint8_t visited[GRID_SIZE * GRID_SIZE];
    std::memset(visited, 0, sizeof(visited));
    int queue[GRID_SIZE * GRID_SIZE];
    int qh = 0, qt = 0;
    queue[qt++] = headIdx;
    visited[headIdx] = 1;

    static const int dx[4] = {0, 0, -1, 1};
    static const int dy[4] = {-1, 1, 0, 0};

    int reachable = 0;
    while (qh < qt) {
        const int cur = queue[qh++];
        ++reachable;
        const int cx = cur % GRID_SIZE;
        const int cy = cur / GRID_SIZE;
        for (int d = 0; d < 4; ++d) {
            const int nx = cx + dx[d];
            const int ny = cy + dy[d];
            if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) {
                continue;
            }
            const int ni = ny * GRID_SIZE + nx;
            if (visited[ni] != 0 || blocked[ni] != 0) {
                continue;
            }
            visited[ni] = 1;
            queue[qt++] = ni;
        }
    }

    return reachable >= snakeLenAfter;
}

bool SnakeGameModel::evadeStep() {
    const Position& head = m_snake.front();
    const bool tailFrees = (m_growthCounter == 0);

    // 绕行目标：当前目标零食（不存在时退化为蛇尾方向，稳定追尾）
    Position target = head;
    if (m_currentTargetIndex >= 0 &&
        m_currentTargetIndex < static_cast<int>(m_foods.size()) &&
        !m_foods[m_currentTargetIndex].eaten)
    {
        target = m_foods[m_currentTargetIndex].position;
    }
    const Position& tail = m_snake.back();

    // 在安全移动中优先朝目标逼近（曼哈顿距离最小）；
    // 距离相同时优先朝蛇尾（追尾移动，保证长期存活与稳定绕圈）
    static const int dx[4] = {0, 0, -1, 1};
    static const int dy[4] = {-1, 1, 0, 0};

    int bestDir = -1;
    int bestTargetDist = INT_MAX;
    int bestTailDist = INT_MAX;

    for (int d = 0; d < 4; ++d) {
        Position cand;
        cand.x = head.x + dx[d];
        cand.y = head.y + dy[d];
        if (!isEvadeMoveSafe(cand, tailFrees)) {
            continue;
        }
        const int tDist = manhattanDistance(cand, target);
        if (tDist < bestTargetDist) {
            bestTargetDist = tDist;
            bestTailDist = manhattanDistance(cand, tail);
            bestDir = d;
        } else if (tDist == bestTargetDist) {
            const int tailDist = manhattanDistance(cand, tail);
            if (tailDist < bestTailDist) {
                bestTailDist = tailDist;
                bestDir = d;
            }
        }
    }

    if (bestDir < 0) {
        return false; // 所有方向均不安全
    }

    Position newHead;
    newHead.x = head.x + dx[bestDir];
    newHead.y = head.y + dy[bestDir];
    applyMove(newHead, false); // 绕行移动不吃食物（isEvadeMoveSafe 已排除食物格）
    return true;
}

void SnakeGameModel::checkFoodCollision() {
    Position head = m_snake.front();

    // 检查蛇头是否与任一未被吃的食物重合
    for (auto& food : m_foods) {
        if (!food.eaten && food.position == head) {
            // 吃到食物！
            food.eaten = true;
            m_score += food.weight;
            m_eatenCount++;
            emit foodEaten();  // 吃食反馈信号（音效等）

            // 蛇身增长：增加growthCounter
            // 注意：moveStep()中已经插入了新蛇头但没有移除尾部
            // growthCounter会在后续的moveStep()中控制是否继续保留尾部
            // 由于moveStep()已执行完毕（已插入头未移除尾），
            // 我们需要为后续的移动设置增长计数
            m_growthCounter += food.weight;

            // 检查是否所有食物都已吃完
            if (m_eatenCount >= FOOD_COUNT) {
                m_state = GameState::WIN;
                emit gameStateChanged(m_state);
                emit gameOver(m_score);
                return;
            }

            // 选择下一个目标并计算路径
            selectNextTarget();
            computePath();

            return; // 一次tick只吃一个食物
        }
    }
}

int SnakeGameModel::manhattanDistance(const Position& a, const Position& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

std::vector<int> SnakeGameModel::generateWeights() {
    std::vector<int> weights;
    weights.reserve(static_cast<size_t>(FOOD_COUNT));
    int currentTotalWeight = 0;

    // 使用正态分布生成权重（保持与 shuffle 共用 m_rng 的抽取序列不变）
    std::normal_distribution<double> distribution(WEIGHT_MEAN, WEIGHT_STDDEV);

    for (int i = 0; i < FOOD_COUNT; ++i) {
        // 生成权重值：正态分布 → 舍入为整数 → 最小值为1
        int weight = static_cast<int>(std::round(distribution(m_rng)));
        if (weight < 1) {
            weight = 1;
        }
        weights.push_back(weight);
        currentTotalWeight += weight;
    }

    // 调整权重使总和不超过MAX_TOTAL_WEIGHT。
    // 优化：原实现反复 max_element 扫描并逐 1 递减 -> O(excess*n) 随机访问；
    // 现按索引降序排序，单次顺序削减最大权重 -> O(n log n)，一次线性扫描。
    int excess = currentTotalWeight - MAX_TOTAL_WEIGHT;
    if (excess > 0) {
        // 按权重降序排列索引（weights 本体保持原位）
        std::vector<int> idx(static_cast<size_t>(FOOD_COUNT));
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&weights](int a, int b) {
            return weights[static_cast<size_t>(a)] > weights[static_cast<size_t>(b)];
        });

        // 从最大权重向下削减，每个权重最低可减到 1
        for (int pos = 0; pos < FOOD_COUNT && excess > 0; ++pos) {
            int& w = weights[static_cast<size_t>(idx[static_cast<size_t>(pos)])];
            int reducible = w - 1;
            if (reducible > 0) {
                int cut = (excess < reducible) ? excess : reducible;
                w -= cut;
                excess -= cut;
            }
        }
    }

    return weights;
}

// ==================== 手动模式 ====================

void SnakeGameModel::manualTick() {
    const Position& head = m_snake.front();
    Position newHead;
    newHead.x = head.x + m_dirX;
    newHead.y = head.y + m_dirY;

    // 撞墙判定
    if (newHead.x < 0 || newHead.x >= GRID_SIZE ||
        newHead.y < 0 || newHead.y >= GRID_SIZE)
    {
        endManualGame();
        return;
    }

    // 探测新蛇头是否吃到食物
    int eatenFoodIndex = -1;
    for (size_t i = 0; i < m_foods.size(); ++i) {
        const FoodItem& food = m_foods[i];
        if (!food.eaten && food.position.x == newHead.x && food.position.y == newHead.y) {
            eatenFoodIndex = static_cast<int>(i);
            break;
        }
    }
    const bool ateFood = (eatenFoodIndex >= 0);

    // 撞自身判定：吃到食物时尾部保留（蛇身增长），须提前判死
    if (m_occupancy[newHead.y * GRID_SIZE + newHead.x] != 0) {
        const bool tailFrees = !ateFood && m_growthCounter == 0;
        if (!tailFrees) {
            endManualGame();
            return;
        }
        const Position& tail = m_snake.back();
        if (newHead.x != tail.x || newHead.y != tail.y) {
            endManualGame();
            return;
        }
    }

    // 困难模式违规判定：场上仍有更小未吃零食时吃到更大零食 → 直接判负
    if (ateFood && m_gameMode == GameMode::HARD) {
        const int eatenWeight = m_foods[static_cast<size_t>(eatenFoodIndex)].weight;
        for (const FoodItem& food : m_foods) {
            if (!food.eaten && food.weight < eatenWeight) {
                endManualGame();
                return;
            }
        }
    }

    // 执行移动（吃到食物保留蛇尾，增长由 handleManualFoodEaten 累计）
    applyMove(newHead, ateFood);

    if (ateFood) {
        handleManualFoodEaten(eatenFoodIndex);
    }

    emit gameDataUpdated();
}

void SnakeGameModel::handleManualFoodEaten(int foodIndex) {
    FoodItem& food = m_foods[static_cast<size_t>(foodIndex)];
    food.eaten = true;
    m_score += food.weight;
    m_eatenCount++;
    emit foodEaten();  // 吃食反馈信号（音效等）
    // 蛇身增长：保留尾部，后续移动由 m_growthCounter 控制是否继续保留
    m_growthCounter += food.weight;

    if (m_gameMode == GameMode::CLASSIC) {
        // 常见模式：零食无限刷新，蛇身铺满地图则胜利
        spawnFood();
        if (m_snake.size() >= static_cast<size_t>(GRID_SIZE * GRID_SIZE)) {
            m_state = GameState::WIN;
            emit gameStateChanged(m_state);
            emit gameOver(m_score);
        }
        return;
    }

    // 正常/困难模式：吃完 20 个零食胜利
    if (m_eatenCount >= FOOD_COUNT) {
        m_state = GameState::WIN;
        emit gameStateChanged(m_state);
        emit gameOver(m_score);
    }
}

void SnakeGameModel::spawnFood() {
    // 第一步：O(20) 标记未吃零食占用的格子
    // （原实现为 625×20 双重循环，改为两次线性遍历，量级不变但常数更低）
    uint8_t foodOcc[GRID_SIZE * GRID_SIZE] = {};
    for (const FoodItem& food : m_foods) {
        if (!food.eaten) {
            foodOcc[food.position.y * GRID_SIZE + food.position.x] = 1;
        }
    }

    // 第二步：O(625) 收集空白位置（蛇身占用 + 未吃食物占用除外）
    std::vector<Position> emptyPositions;
    emptyPositions.reserve(static_cast<size_t>(GRID_SIZE * GRID_SIZE));
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            if (m_occupancy[y * GRID_SIZE + x] != 0) {
                continue;
            }
            if (foodOcc[y * GRID_SIZE + x] != 0) {
                continue;
            }
            emptyPositions.push_back({x, y});
        }
    }
    if (emptyPositions.empty()) {
        return;  // 已铺满，胜利判定在 handleManualFoodEaten 中处理
    }

    // 随机选择一个空白位置（正态权重，与 generateWeights 同分布）
    std::uniform_int_distribution<size_t> posDist(0, emptyPositions.size() - 1);
    Position pos = emptyPositions[posDist(m_rng)];

    std::normal_distribution<double> distribution(WEIGHT_MEAN, WEIGHT_STDDEV);
    int weight = static_cast<int>(std::round(distribution(m_rng)));
    if (weight < 1) {
        weight = 1;
    }

    // 复用已吃槽位，保持场上零食数恒定
    for (FoodItem& food : m_foods) {
        if (food.eaten) {
            food.position = pos;
            food.weight = weight;
            food.eaten = false;
            m_totalFoodSpawned++;  // 刷新零食，累计出现总数 +1
            return;
        }
    }
    // 兜底：无已吃槽位（理论上不会发生）则追加
    FoodItem food;
    food.position = pos;
    food.weight = weight;
    food.eaten = false;
    m_foods.push_back(food);
    m_totalFoodSpawned++;  // 兜底刷新同样累计
}

void SnakeGameModel::endManualGame() {
    m_state = GameState::GAME_OVER;
    emit gameStateChanged(m_state);
    emit gameOver(m_score);
    emit gameDataUpdated();
}
