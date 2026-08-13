/**
 * @file SnakeGameModel.h
 * @brief 贪吃蛇游戏模型层
 *
 * 负责管理所有游戏状态：25×25网格、蛇身坐标、20个带权重的零食、
 * A*寻路集成、游戏逻辑（移动、碰撞检测、得分统计）。
 * 通过Qt信号通知视图层更新。
 */

#ifndef SNAKEMODEL_H
#define SNAKEMODEL_H

#include <QObject>
#include <QString>
#include <deque>
#include <vector>
#include <utility>
#include <random>
#include <cstdint>

// ==================== 常量定义 ====================

/// 游戏网格大小（25×25）
const int GRID_SIZE = 25;
/// 同时存在的零食数量
const int FOOD_COUNT = 20;
/// 蛇的初始长度（固定测试布局使用）
const int INITIAL_SNAKE_LENGTH = 1;
/// 随机开局布局的蛇长度（头 + 沿初始方向 2 节身体）
const int RANDOM_SNAKE_LENGTH = 3;
/// 零食权重正态分布的均值
const double WEIGHT_MEAN = 3.0;
/// 零食权重正态分布的标准差
const double WEIGHT_STDDEV = 1.5;
/// 地图总容量 = GRID_SIZE * GRID_SIZE - 1（蛇头占一个格子）
const int MAX_TOTAL_WEIGHT = GRID_SIZE * GRID_SIZE - 1;
/// 游戏循环基准间隔（毫秒）：中速档基准，也是 BASE 兜底值
const int BASE_TICK_INTERVAL = 150;
/// 手动模式加速下限（毫秒）
const int MIN_TICK_INTERVAL = 80;
/// 排行榜最大记录数（Top10）
const int LEADERBOARD_SIZE = 10;

// ==================== 数据类型定义 ====================

/**
 * @brief 位置结构体
 *
 * 表示网格中的一个坐标点，x为列号，y为行号。
 */
struct Position {
    int x;  ///< 列号（水平方向，范围[0, GRID_SIZE)）
    int y;  ///< 行号（垂直方向，范围[0, GRID_SIZE)）

    /// 相等比较运算符
    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
    /// 不等比较运算符
    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
};

/**
 * @brief 食物结构体
 *
 * 每个零食具有位置、权重值和是否已被吃掉的状态。
 */
struct FoodItem {
    Position position;  ///< 食物在网格中的位置
    int weight;          ///< 食物的权重值（正整数，由正态分布生成）
    bool eaten;          ///< 是否已被蛇吃掉
};

/**
 * @brief 游戏状态枚举
 *
 * IDLE:     游戏未开始（初始状态）
 * PLAYING:  游戏进行中（蛇正在移动）
 * PAUSED:   游戏已暂停
 * GAME_OVER:游戏结束（蛇无路可走）
 * WIN:      游戏胜利（所有零食已吃完）
 */
enum class GameState {
    IDLE,
    PLAYING,
    PAUSED,
    GAME_OVER,
    WIN
};

/**
 * @brief 玩法模式（手动操作时生效；自动模式恒按权重优先规则）
 *
 * CLASSIC: 常见模式 - 一般贪吃蛇规则。零食带权重（蛇身按权重增长），
 *          吃一个刷一个新的（零食无限刷新），蛇身铺满地图则胜利。
 * NORMAL:  正常模式 - 取消"优先小零食/不可吃大零食"的限制，
 *          大小零食均可吃，长度按零食权重增加，吃完 20 个零食胜利。
 * HARD:    困难模式 - 场上仍有更小未吃零食时，吃到更大零食直接判负。
 */
enum class GameMode {
    CLASSIC,
    NORMAL,
    HARD
};

/**
 * @brief 操作方式
 *
 * AUTO:   自动模式 - 蛇由 A* 算法自动寻路（原有权重优先 AI，保持不变）。
 * MANUAL: 手动模式 - 蛇由玩家方向键/WASD 控制，按所选玩法模式判定规则。
 */
enum class ControlMode {
    AUTO,
    MANUAL
};

/**
 * @brief 游戏速度档位（自动/手动模式均生效）
 *
 * SLOW:   慢速 - 基准间隔 220ms
 * NORMAL: 中速 - 基准间隔 150ms（默认）
 * FAST:   快速 - 基准间隔 90ms
 *
 * 自动模式恒定按档位基准运行；手动模式以此为起点随成长加速。
 */
enum class SpeedLevel {
    SLOW,
    NORMAL,
    FAST
};

/**
 * @brief 排行榜单条记录
 *
 * 一局结束（胜利或失败）后写入；按得分降序截断 Top10。
 */
struct LeaderboardEntry {
    int score;           ///< 本局得分
    int seconds;         ///< 本局用时（秒）
    int eaten;           ///< 已吃零食数
    QString date;        ///< 完成时间（yyyy-MM-dd hh:mm）

    /// 相等比较（供 std::find 定位本局新记录）
    bool operator==(const LeaderboardEntry& other) const {
        return score == other.score && seconds == other.seconds && eaten == other.eaten;
    }
};

// ==================== 游戏模型类 ====================

/**
 * @brief 贪吃蛇游戏模型
 *
 * Model-View架构中的Model层，管理所有游戏数据和逻辑。
 * 不包含任何UI代码，通过Qt信号通知View层数据变化。
 */
class SnakeGameModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent Qt父对象
     */
    explicit SnakeGameModel(QObject* parent = nullptr);

    /**
     * @brief 重置游戏到初始状态
     *
     * 清空蛇身、重新生成零食、重置得分。
     * 默认使用随机开局布局（蛇头位置/方向/长度每次不同）；
     * 调用过 setFixedStartLayout() 后保持固定测试布局（跨 reset 持续生效）。
     */
    void reset();

    /**
     * @brief 开始游戏
     *
     * 从IDLE状态切换到PLAYING状态，选择第一个目标并计算路径。
     */
    void start();

    /**
     * @brief 切换暂停/恢复状态
     *
     * PLAYING → PAUSED 或 PAUSED → PLAYING
     */
    void togglePause();

    /**
     * @brief 执行一步游戏逻辑
     *
     * 由Controller的QTimer每150ms调用一次。
     * 自动模式：A*寻路驱动（原有逻辑）；手动模式：方向键驱动。
     * 负责：蛇移动一步、碰撞检测、食物检测、模式规则判定。
     */
    void tick();

    // ==================== 模式与手动控制接口 ====================

    /**
     * @brief 设置玩法模式（常见/正常/困难）
     * 仅在 IDLE 状态生效（游戏中禁止中途切换）。
     */
    void setGameMode(GameMode mode);

    /// 获取当前玩法模式
    GameMode getGameMode() const { return m_gameMode; }

    /**
     * @brief 设置操作方式（自动 AI / 手动方向键）
     * 仅在 IDLE 状态生效。
     */
    void setControlMode(ControlMode mode);

    /// 获取当前操作方式
    ControlMode getControlMode() const { return m_controlMode; }

    /**
     * @brief 手动模式：设置蛇的移动方向（仅 MANUAL 模式生效）
     * 禁止 180° 直接掉头（防止瞬间反向撞到自身）。
     * @param dx 水平方向分量（-1/0/1）
     * @param dy 垂直方向分量（-1/0/1）
     */
    void setMoveDirection(int dx, int dy);

    /// 手动模式当前移动方向 X 分量（供 UI/测试查询）
    int getDirectionX() const { return m_dirX; }

    /// 手动模式当前移动方向 Y 分量（供 UI/测试查询）
    int getDirectionY() const { return m_dirY; }

    // ==================== 状态查询接口 ====================

    /// 获取当前游戏状态
    GameState getGameState() const { return m_state; }

    /// 获取蛇身坐标序列（头部在front，尾部在back）
    const std::deque<Position>& getSnake() const { return m_snake; }

    /// 获取所有食物列表
    const std::vector<FoodItem>& getFoods() const { return m_foods; }

    /// 获取当前得分（已吃零食的权重之和）
    int getScore() const { return m_score; }

    /// 获取已吃零食数量
    int getEatenCount() const { return m_eatenCount; }

    /// 获取本局累计出现的零食总数（初始FOOD_COUNT + 刷新次数，作为进度分母基准）
    int getTotalFoodCount() const { return m_totalFoodSpawned; }

    /// 获取当前目标零食的权重（无目标时返回0）
    int getCurrentTargetWeight() const;

    /// 获取网格中某位置是否被蛇身占据（包含蛇头）
    bool isOccupied(int x, int y) const;

    // ==================== 速度与记录 ====================

    /**
     * @brief 设置游戏速度档位（跨局持久化，QSettings 存储）
     * 自动/手动模式均按档位生效（自动模式恒定按档位基准，手动模式随成长加速）。
     */
    void setSpeedLevel(SpeedLevel level);

    /// 获取当前速度档位
    SpeedLevel getSpeedLevel() const { return m_speedLevel; }

    /// 获取当前推荐的 tick 间隔（毫秒）：
    /// 自动模式恒定按档位基准；手动模式按档位基准间隔随成长加速
    /// （每吃 10 个快一档，下限 MIN_TICK_INTERVAL）
    int getRecommendedTickInterval() const;

    /// 获取历史最高分（跨局持久化，QSettings 存储）
    int getBestScore() const { return m_bestScore; }

    /// 更新最高分：若 score 超过当前记录则保存并返回 true（新纪录），否则返回 false
    bool updateBestScore(int score);

    // ==================== 排行榜（Top10） ====================

    /**
     * @brief 获取排行榜记录（按得分降序，最多 LEADERBOARD_SIZE 条）
     * 跨局持久化，QSettings 存储。
     */
    std::vector<LeaderboardEntry> getLeaderboard() const;

    /**
     * @brief 将一局记录写入排行榜（按得分降序，截断 Top10）
     * @return true 表示该记录进入排行榜（含并列场景），false 表示未入榜
     */
    bool addLeaderboardEntry(int score, int seconds, int eaten);

    /// 是否处于"让位模式"（所有目标不可达时 AI 绕行腾出通道，供 UI 显示状态）
    bool isEvading() const { return m_evading; }

    /**
     * @brief 固定开局布局（仅供单元测试使用）
     *
     * 默认开局为随机布局（蛇头位置/方向/长度随机，每局不同）。
     * 测试调用本方法后，reset() 使用确定性布局：蛇头居中、长度 1、方向向右，
     * 与历史测试语义保持一致。设置一次后跨多次 reset 持续生效。
     */
    void setFixedStartLayout();

signals:
    /**
     * @brief 游戏状态改变信号
     * @param newState 新的游戏状态
     */
    void gameStateChanged(GameState newState);

    /**
     * @brief 游戏数据更新信号（蛇移动、食物变化等）
     */
    void gameDataUpdated();

    /**
     * @brief 吃到零食信号（用于播放吃食音效等反馈）
     */
    void foodEaten();

    /**
     * @brief 游戏结束信号
     * @param finalScore 最终得分
     */
    void gameOver(int finalScore);

private:
    // ==================== 核心数据 ====================

    std::deque<Position> m_snake;           ///< 蛇身坐标序列，头部在deque前端
    std::vector<FoodItem> m_foods;           ///< 所有食物列表
    GameState m_state;                       ///< 当前游戏状态
    int m_score;                             ///< 当前得分
    int m_eatenCount;                        ///< 已吃零食数量
    int m_totalFoodSpawned;                  ///< 本局累计出现的零食总数（初始FOOD_COUNT + 刷新次数）
    int m_bestScore;                         ///< 历史最高分（QSettings 持久化）
    SpeedLevel m_speedLevel;                 ///< 速度档位（QSettings 持久化，自动/手动模式均生效）
    int m_currentTargetIndex;                ///< 当前目标零食索引（-1表示无目标）
    std::vector<std::pair<int, int>> m_currentPath;  ///< 当前A*计算出的路径
    size_t m_pathIndex;                      ///< 当前路径中的移动步数索引
    int m_growthCounter;                     ///< 蛇身增长计数器（吃零食后累积）

    std::default_random_engine m_rng;        ///< 随机数引擎
    std::uniform_int_distribution<int> m_posDist;  ///< 位置分布[0, GRID_SIZE-1]

    // 让位模式标志（Issue 2）：当前所有目标均不可达时置 true。
    // 蛇改为执行安全绕行移动（兜圈子/折叠自身），逐步腾出通向目标的通道，
    // 每次绕行后重试寻路，通道出现即自动切回正常寻路模式。
    bool m_evading;

    // 固定开局布局标志：true 时 initSnake 使用确定性布局（测试专用）；
    // false（默认）时每次 reset 生成随机开局布局。
    bool m_fixedStartLayout;

    // ==================== 手动操作状态 ====================

    GameMode m_gameMode;         ///< 玩法模式（常见/正常/困难），手动模式生效
    ControlMode m_controlMode;   ///< 操作方式（自动 AI / 手动方向键）
    int m_dirX;                  ///< 手动模式当前移动方向 X 分量
    int m_dirY;                  ///< 手动模式当前移动方向 Y 分量

    // 蛇身占用网格（缓存优化核心）：
    // uint8_t 扁平数组，m_occupancy[y * GRID_SIZE + x] = 1 表示该格子被蛇占据。
    // 625 字节 ≈ 10 个 cache line，常驻 L1；isOccupied 由 O(n) deque 扫描
    // 降为 O(1) 查表。与 m_snake 在 push_front/pop_back/reset 时保持同步。
    uint8_t m_occupancy[GRID_SIZE * GRID_SIZE];

    // ==================== 内部方法 ====================

    /// 初始化蛇开局布局：默认随机（位置/方向/长度每次不同）；
    /// setFixedStartLayout() 后使用确定性布局（中央单节向右）
    void initSnake();

    /// 生成20个带权重的随机食物
    void generateFoods();

    /// 构建扁平化障碍网格（蛇身=障碍，蛇头=可通行），供A*直接消费
    /// 行主序：out[y * GRID_SIZE + x]，非零=障碍
    void buildObstacleFlat(uint8_t out[GRID_SIZE * GRID_SIZE]) const;

    /// 构建针对指定目标的扁平化障碍网格（Issue 1）：
    /// 蛇身 + 除目标外的所有未吃零食均为障碍 + 蛇头可通行。
    /// 使A*只规划"空白路径点"的最优路线，绝不顺路经过非目标零食。
    /// @param foodIndex 目标零食索引（调用方保证其有效且未吃）
    void buildObstacleFlatForTarget(int foodIndex, uint8_t out[GRID_SIZE * GRID_SIZE]) const;

    /// 计算到指定食物的A*路径（复用已构建的障碍网格）
    /// @param foodIndex 目标食物索引（调用方保证其有效且未吃）
    /// @param obstacleFlat 已构建的扁平障碍网格
    void computePathTo(int foodIndex, const uint8_t* obstacleFlat);

    /// 两级寻路（Issue 1）：先以"非目标零食为障碍"的严格模式找纯空白路径；
    /// 严格不可达时回退到宽松模式（仅蛇身为障碍）兜底，保证游戏可推进。
    /// @return true 表示成功获得路径（m_currentPath 已更新）
    bool tryComputePathToTarget(int foodIndex);

    /// 执行一步安全绕行移动（Issue 2 让位模式）：
    /// 在全部 4 个方向中筛选安全移动（flood-fill 判据），
    /// 优先朝目标方向逼近，其次朝蛇尾方向追尾（保证长期存活）。
    /// @return false 表示所有方向均不安全（真正死局，应判定 GAME_OVER）
    bool evadeStep();

    /// 安全移动判据：模拟从当前蛇头移动到 newHead 后，
    /// flood-fill 统计新蛇头可达的自由格数，须不小于移动后蛇身长度。
    /// 这是"兜圈子不自杀"的充分条件（蛇有足够空间弯曲而不撞自己）。
    /// @param newHead 候选新蛇头位置
    /// @param tailFrees 本次移动后蛇尾是否腾出（不吃食物且增长计数器为 0）
    bool isEvadeMoveSafe(const Position& newHead, bool tailFrees) const;

    /// 实际执行一步移动：插入新蛇头 + 同步占用网格；
    /// 未吃到食物时按增长计数器决定是否移除蛇尾（同步清除占用位）。
    /// @param newHead 新蛇头位置
    /// @param ateFood 本次移动是否吃到食物（吃到则保留蛇尾，增长由 checkFoodCollision 累计）
    void applyMove(const Position& newHead, bool ateFood);

    /// 自动模式单步：A* 寻路 + 让位绕行（原 tick 逻辑）
    void autoTick();

    /// 手动模式单步：按当前方向移动 + 撞墙/撞身/难度规则判定 + 吃食物处理
    void manualTick();

    /// 手动模式吃食物后的通用处理（得分/增长/胜利判定）
    /// @param foodIndex 被吃食物索引（调用方保证有效）
    void handleManualFoodEaten(int foodIndex);

    /// 常见模式：在随机空白位置刷新一个新零食（复用已吃槽位，保持场上零食数恒定）
    void spawnFood();

    /// 判定并进入 GAME_OVER（手动模式统一出口）
    void endManualGame();

    /// 选择下一个目标零食（权重最小为主规则；同权重候选中做前瞻选择：
    /// 优先"吃掉候选后距次小权重零食最近"者，缩短前往下一目标的衔接距离）
    void selectNextTarget();

    /// 计算到当前目标的A*路径
    void computePath();

    /// 执行一步移动（沿路径前进一步）
    void moveStep();

    /// 检查蛇头是否吃到食物
    void checkFoodCollision();

    /// 计算两个位置之间的曼哈顿距离
    static int manhattanDistance(const Position& a, const Position& b);

    /// 生成20个零食权重（正态分布，舍入为整数，最小值1）
    std::vector<int> generateWeights();
};

#endif // SNAKEMODEL_H
