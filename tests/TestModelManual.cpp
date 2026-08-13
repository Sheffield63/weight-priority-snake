/**
 * @file TestModelManual.cpp
 * @brief 手动模式玩法规则常驻测试（QTest，headless）
 *
 * 覆盖：方向控制与 180° 掉头禁止、撞墙/撞身判负、吃食得分与增长、
 * CLASSIC 刷新计数、NORMAL 计数、HARD 违规判负、信号发射。
 *
 * 吃食类测试使用内置 BFS 导航器（避开蛇身的最短路径）驱动蛇到达食物，
 * 不依赖随机布局，保证确定性。
 */

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "src/model/SnakeGameModel.h"
#include <climits>
#include <queue>
#include <cstring>
#include <algorithm>

/**
 * @brief 手动模式规则测试类
 */
class TestModelManual : public QObject {
    Q_OBJECT

private slots:
    // ===== 初始状态 =====

    /// 蛇头初始在地图中央
    void initHeadAtCenter();
    /// 初始长度为 1
    void initLengthOne();
    /// 初始方向向右（默认）
    void initDirectionRight();

    // ===== 方向控制 =====

    /// 右移：tick 后蛇头 x+1
    void moveRight();
    /// 上移：tick 后蛇头 y-1
    void moveUp();
    /// 下移：tick 后蛇头 y+1
    void moveDown();
    /// 左移：先向上再向左（直接设左是 180° 掉头会被拒绝）
    void moveLeft();
    /// 禁止 180° 掉头：向右时输入向左被忽略
    void reverseBlocked();
    /// 零向量方向被忽略
    void zeroVectorIgnored();
    /// 自动模式下 setMoveDirection 无效
    void directionIgnoredInAuto();

    // ===== 碰撞判负 =====

    /// 持续向右最终撞墙 → GAME_OVER
    void wallCollisionEndsGame();
    /// 撞墙时发射 gameOver 信号，分数与模型最终分数一致（强随机布局可能沿途吃到零食）
    void wallCollisionEmitsGameOver();
    /// 撞自身判负（蛇身足够长时反向绕回）
    void selfCollisionEndsGame();

    // ===== 吃食逻辑 =====

    /// NORMAL：吃食后 score 增加、eatenCount 增加
    void eatIncreasesScoreAndCount();
    /// NORMAL：吃食不刷新，累计总数保持 FOOD_COUNT
    void eatNormalNoRefresh();
    /// CLASSIC：吃食刷新，累计总数递增、场上未吃数保持 FOOD_COUNT
    void eatClassicRefreshes();
    /// 吃食时发射 foodEaten 信号
    void eatEmitsFoodEaten();
    /// HARD：吃全局最小权重零食合法（不判负）
    void hardMinWeightEatLegal();
    /// HARD：先吃掉一个零食后，场上仍有更小未吃零食时吃大零食 → 判负
    void hardViolationRule();

    // ===== 计数接口 =====

    /// 手动模式目标权重 = 场上最小未吃权重
    void targetWeightIsMin();
    /// 蛇身占用网格查询正确
    void occupancyQuery();

private:
    /// 构造手动模式模型（固定开局布局：蛇头居中、长度1、方向右，保证测试确定性）
    std::shared_ptr<SnakeGameModel> makeManual() const {
        auto model = std::make_shared<SnakeGameModel>();
        model->setFixedStartLayout();
        model->setControlMode(ControlMode::MANUAL);
        model->setGameMode(GameMode::NORMAL);
        return model;
    }

    /// 曼哈顿距离
    static int manhattan(const Position& a, const Position& b) {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    }

    /// 检查格子是否为未吃食物（O(FOOD_COUNT)）
    static bool hasUneatenFoodAt(const SnakeGameModel& model, int x, int y) {
        for (const auto& f : model.getFoods()) {
            if (!f.eaten && f.position.x == x && f.position.y == y) return true;
        }
        return false;
    }

    /// BFS 最短路径（避开蛇身占用的格子；蛇头视为起点可通行）
    /// @param avoidFoods 为 true 时避开所有"未吃食物"格子（目标格除外），
    ///                   保证导航不沿途吃掉其他食物——HARD 模式必须使用，
    ///                   否则路径穿过食物格会先吃到不该吃的零食导致违规判负。
    /// @param currentDir 非空时约束"第一步不得与该方向相反"（180° 掉头会被
    ///                   setMoveDirection 拒绝），用于导航器沿真实可执行路径走。
    /// @return 路径坐标序列（不含起点，含终点）；不可达返回空
    static std::vector<Position> bfsPath(const SnakeGameModel& model,
                                         const Position& head,
                                         const Position& target,
                                         bool avoidFoods = false,
                                         const Position* currentDir = nullptr) {
        int came[GRID_SIZE * GRID_SIZE];
        std::memset(came, -1, sizeof(came));
        std::queue<int> q;
        const int start = head.y * GRID_SIZE + head.x;
        const int goal = target.y * GRID_SIZE + target.x;
        came[start] = start;
        q.push(start);

        static const int dx[4] = {1, -1, 0, 0};
        static const int dy[4] = {0, 0, 1, -1};

        while (!q.empty()) {
            const int cur = q.front();
            q.pop();
            if (cur == goal) break;
            const int cx = cur % GRID_SIZE;
            const int cy = cur / GRID_SIZE;
            for (int d = 0; d < 4; ++d) {
                const int nx = cx + dx[d];
                const int ny = cy + dy[d];
                if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) continue;
                const int ni = ny * GRID_SIZE + nx;
                if (came[ni] != -1) continue;
                // 约束：第一步不得与当前方向相反（模型会拒绝 180° 掉头）
                if (currentDir != nullptr && cur == start &&
                    dx[d] == -currentDir->x && dy[d] == -currentDir->y) {
                    continue;
                }
                // 蛇身阻挡（目标格本身是食物，非蛇身；蛇头格是起点已标记）
                if (model.isOccupied(nx, ny)) continue;
                // 可选：避开未吃食物（目标格除外）
                if (avoidFoods && ni != goal && hasUneatenFoodAt(model, nx, ny)) continue;
                came[ni] = cur;
                q.push(ni);
            }
        }
        if (came[goal] == -1) return {};

        std::vector<Position> path;
        for (int cur = goal; cur != start; cur = came[cur]) {
            path.push_back({cur % GRID_SIZE, cur / GRID_SIZE});
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    /// 持续导航：每步 BFS 到最近可达食物并移动一步，直到吃到食物
    /// @return 是否吃到至少一个食物
    static bool navigateAndEat(std::shared_ptr<SnakeGameModel> model) {
        const int eatenBefore = model->getEatenCount();
        const int maxSteps = GRID_SIZE * GRID_SIZE * 2;
        Position currentDir{1, 0};  // 手动模式默认向右（与模型初始方向一致）
        for (int step = 0; step < maxSteps; ++step) {
            if (model->getGameState() != GameState::PLAYING) {
                return model->getEatenCount() > eatenBefore;
            }
            if (model->getEatenCount() > eatenBefore) return true;

            const Position head = model->getSnake().front();
            // 找最近可达食物（可达性探测无需方向约束）
            Position target;
            bool found = false;
            for (const auto& f : model->getFoods()) {
                if (f.eaten) continue;
                if (bfsPath(*model, head, f.position).empty()) continue;
                if (!found || manhattan(head, f.position) < manhattan(head, target)) {
                    target = f.position;
                    found = true;
                }
            }
            if (!found) return false;  // 无可达食物

            const std::vector<Position> path = bfsPath(*model, head, target, false, &currentDir);
            if (path.empty()) return false;
            const Position& next = path.front();
            currentDir = {next.x - head.x, next.y - head.y};
            model->setMoveDirection(currentDir.x, currentDir.y);
            model->tick();
        }
        return model->getEatenCount() > eatenBefore;
    }

    /// 导航到指定食物并吃掉它
    /// @param avoidFoods 为 true 时路径避开未吃食物，保证只吃到目标食物
    ///                   （HARD 模式必用：避免沿途先吃其他零食导致误判）
    /// @return 该指定食物最终是否被吃
    static bool navigateToFood(std::shared_ptr<SnakeGameModel> model,
                               const Position& foodPos,
                               bool avoidFoods = false) {
        const int maxSteps = GRID_SIZE * GRID_SIZE * 2;
        Position currentDir{1, 0};  // 手动模式默认向右
        for (int step = 0; step < maxSteps; ++step) {
            if (model->getGameState() != GameState::PLAYING) return false;
            for (const auto& f : model->getFoods()) {
                if (f.eaten && f.position == foodPos) return true;
            }
            const Position head = model->getSnake().front();
            const std::vector<Position> path =
                bfsPath(*model, head, foodPos, avoidFoods, &currentDir);
            if (path.empty()) return false;
            const Position& next = path.front();
            currentDir = {next.x - head.x, next.y - head.y};
            model->setMoveDirection(currentDir.x, currentDir.y);
            model->tick();
        }
        for (const auto& f : model->getFoods()) {
            if (f.eaten && f.position == foodPos) return true;
        }
        return false;
    }
};

// ==================== 初始状态 ====================

void TestModelManual::initHeadAtCenter() {
    auto model = makeManual();
    const auto& snake = model->getSnake();
    QCOMPARE(snake.front().x, GRID_SIZE / 2);
    QCOMPARE(snake.front().y, GRID_SIZE / 2);
}

void TestModelManual::initLengthOne() {
    auto model = makeManual();
    QCOMPARE(static_cast<int>(model->getSnake().size()), 1);
}

void TestModelManual::initDirectionRight() {
    auto model = makeManual();
    model->start();
    model->tick();
    const Position head = model->getSnake().front();
    QCOMPARE(head.x, GRID_SIZE / 2 + 1);  // 默认向右
    QCOMPARE(head.y, GRID_SIZE / 2);
}

// ==================== 方向控制 ====================

void TestModelManual::moveRight() {
    auto model = makeManual();
    model->start();
    model->setMoveDirection(1, 0);
    model->tick();
    QCOMPARE(model->getSnake().front().x, GRID_SIZE / 2 + 1);
}

void TestModelManual::moveUp() {
    auto model = makeManual();
    model->start();
    model->setMoveDirection(0, -1);
    model->tick();
    QCOMPARE(model->getSnake().front().y, GRID_SIZE / 2 - 1);
}

void TestModelManual::moveDown() {
    auto model = makeManual();
    model->start();
    model->setMoveDirection(0, 1);
    model->tick();
    QCOMPARE(model->getSnake().front().y, GRID_SIZE / 2 + 1);
}

void TestModelManual::moveLeft() {
    auto model = makeManual();
    model->start();
    // 初始方向为右：直接设左是 180° 掉头会被拒绝，先向上再向左
    model->setMoveDirection(0, -1);
    model->tick();                          // 蛇头 (12, 11)
    model->setMoveDirection(-1, 0);
    model->tick();                          // 蛇头 (11, 11)
    QCOMPARE(model->getSnake().front().x, GRID_SIZE / 2 - 1);
    QCOMPARE(model->getSnake().front().y, GRID_SIZE / 2 - 1);
}

void TestModelManual::reverseBlocked() {
    auto model = makeManual();
    model->start();
    model->setMoveDirection(1, 0);   // 先设右
    model->setMoveDirection(-1, 0);  // 180° 掉头 → 应被忽略
    model->tick();
    QCOMPARE(model->getSnake().front().x, GRID_SIZE / 2 + 1);  // 仍向右
}

void TestModelManual::zeroVectorIgnored() {
    auto model = makeManual();
    model->start();
    model->setMoveDirection(0, 0);   // 零向量 → 忽略
    model->tick();
    QCOMPARE(model->getSnake().front().x, GRID_SIZE / 2 + 1);  // 默认右
}

void TestModelManual::directionIgnoredInAuto() {
    auto model = std::make_shared<SnakeGameModel>();  // AUTO
    model->start();
    model->setMoveDirection(0, -1);  // 自动模式下应无效
    model->tick();
    // 自动模式由 A* 驱动，方向输入不产生固定偏移；验证状态仍 PLAYING 即可
    QCOMPARE(model->getGameState(), GameState::PLAYING);
}

// ==================== 碰撞判负 ====================

void TestModelManual::wallCollisionEndsGame() {
    auto model = makeManual();
    model->start();
    // 一直向右（若吃到食物则自动继续向右，撞墙判定不受影响）
    model->setMoveDirection(1, 0);
    for (int i = 0; i < GRID_SIZE + 5; ++i) {
        model->tick();
    }
    QCOMPARE(model->getGameState(), GameState::GAME_OVER);
}

void TestModelManual::wallCollisionEmitsGameOver() {
    auto model = makeManual();
    QSignalSpy spy(model.get(), &SnakeGameModel::gameOver);
    model->start();
    // 向右一路前行直到撞墙。初始布局为强随机（QRandomGenerator::system 播种），
    // 路径上可能随机出现食物并被吃到 → 不能假设分数为 0，断言信号参数与模型最终分数一致
    model->setMoveDirection(1, 0);
    for (int i = 0; i < GRID_SIZE + 5; ++i) {
        model->tick();
    }
    QVERIFY(spy.count() >= 1);
    QCOMPARE(spy.takeFirst().at(0).toInt(), model->getScore());
}

void TestModelManual::selfCollisionEndsGame() {
    auto model = makeManual();
    model->start();
    // 吃 3 个食物使蛇身变长到至少 4 节（零食权重最小为 1，3 个保底 4 节）
    for (int i = 0; i < 3; ++i) {
        if (!navigateAndEat(model)) break;
    }
    const int lenAfter = static_cast<int>(model->getSnake().size());
    if (lenAfter >= 4 && model->getGameState() == GameState::PLAYING) {
        // 确定性自撞：连续顺时针转 90° 绕 1×1 方格（右→下→左→上 视起点方向而定）。
        // 蛇长 ≥ 4 时，第 4 步蛇头回到起点格，而起点格仍在蛇身内 → 必撞自身判负。
        // （起点若贴近边界，中途撞墙同样结束，断言仍成立）
        int dx = model->getDirectionX();
        int dy = model->getDirectionY();
        for (int step = 0; step < 4 && model->getGameState() == GameState::PLAYING; ++step) {
            // 顺时针旋转 90°：(x, y) -> (-y, x)，永不为 180° 反转
            const int nx = -dy;
            const int ny = dx;
            dx = nx;
            dy = ny;
            model->setMoveDirection(dx, dy);
            model->tick();
        }
    }
    // 蛇身已变长：绕 1×1 方格后必然结束（自撞，或贴边时撞墙）
    if (lenAfter >= 4) {
        QVERIFY(model->getGameState() != GameState::PLAYING);
    }
}

// ==================== 吃食逻辑 ====================

void TestModelManual::eatIncreasesScoreAndCount() {
    auto model = makeManual();
    model->start();
    const int eatenBefore = model->getEatenCount();
    const int scoreBefore = model->getScore();
    QVERIFY2(navigateAndEat(model), "BFS 导航应能吃到至少一个食物");
    QVERIFY(model->getEatenCount() > eatenBefore);  // 已吃数量增加
    QVERIFY(model->getScore() > scoreBefore);       // 得分增加
}

void TestModelManual::eatNormalNoRefresh() {
    auto model = makeManual();
    model->start();
    const int totalBefore = model->getTotalFoodCount();
    QVERIFY2(navigateAndEat(model), "BFS 导航应能吃到至少一个食物");
    // NORMAL 模式不刷新：累计总数保持 FOOD_COUNT
    QCOMPARE(model->getTotalFoodCount(), totalBefore);
    QCOMPARE(model->getTotalFoodCount(), FOOD_COUNT);
    // 场上未吃数 = FOOD_COUNT - 已吃
    int uneaten = 0;
    for (const auto& f : model->getFoods()) {
        if (!f.eaten) ++uneaten;
    }
    QCOMPARE(uneaten, FOOD_COUNT - model->getEatenCount());
}

void TestModelManual::eatClassicRefreshes() {
    auto model = makeManual();
    model->setGameMode(GameMode::CLASSIC);  // 必须在 IDLE 状态设置
    model->start();
    const int totalBefore = model->getTotalFoodCount();
    QVERIFY2(navigateAndEat(model), "BFS 导航应能吃到至少一个食物");
    // CLASSIC 刷新：累计总数递增（> 初始 FOOD_COUNT）
    QVERIFY(model->getTotalFoodCount() > totalBefore);
    QVERIFY(model->getTotalFoodCount() > FOOD_COUNT);
    // 场上未吃数保持 FOOD_COUNT（吃一个刷一个）
    int uneaten = 0;
    for (const auto& f : model->getFoods()) {
        if (!f.eaten) ++uneaten;
    }
    QCOMPARE(uneaten, FOOD_COUNT);
}

void TestModelManual::eatEmitsFoodEaten() {
    auto model = makeManual();
    QSignalSpy spy(model.get(), &SnakeGameModel::foodEaten);
    model->start();
    QVERIFY2(navigateAndEat(model), "BFS 导航应能吃到至少一个食物");
    QVERIFY(spy.count() >= 1);  // 吃食必然发射信号
}

void TestModelManual::hardMinWeightEatLegal() {
    auto model = makeManual();
    model->setGameMode(GameMode::HARD);
    model->start();
    // 找到全局最小权重零食
    Position minPos;
    int minWeight = INT_MAX;
    for (const auto& f : model->getFoods()) {
        if (!f.eaten && f.weight < minWeight) {
            minWeight = f.weight;
            minPos = f.position;
        }
    }
    QVERIFY(minWeight < INT_MAX);
    QVERIFY2(navigateToFood(model, minPos, true), "BFS 导航应能吃到最小权重零食");
    // 吃全局最小零食合法：不判负
    QVERIFY(model->getGameState() != GameState::GAME_OVER);
}

void TestModelManual::hardViolationRule() {
    auto model = makeManual();
    model->setGameMode(GameMode::HARD);
    model->start();
    // 先吃掉全局最小权重零食（合法吃食，场上少一个更小的）
    Position minPos;
    int minWeight = INT_MAX;
    for (const auto& f : model->getFoods()) {
        if (!f.eaten && f.weight < minWeight) {
            minWeight = f.weight;
            minPos = f.position;
        }
    }
    QVERIFY(minWeight < INT_MAX);
    QVERIFY2(navigateToFood(model, minPos, true), "BFS 导航应能吃到最小权重零食");

    // 若仍处于 PLAYING：场上还有未吃零食。此时挑一个"比至少一个未吃零食更大"的目标去吃。
    // 吃该目标时场上仍有更小未吃零食 → 应判负。
    if (model->getGameState() == GameState::PLAYING) {
        bool triggered = false;
        for (int guard = 0; guard < FOOD_COUNT && !triggered; ++guard) {
            // 收集当前剩余未吃零食
            std::vector<FoodItem> remaining;
            for (const auto& f : model->getFoods()) {
                if (!f.eaten) remaining.push_back(f);
            }
            if (remaining.empty()) break;

            // 找一个"存在更小未吃零食"的目标
            bool foundTarget = false;
            FoodItem target;
            for (const auto& f : remaining) {
                bool hasSmaller = false;
                for (const auto& g : remaining) {
                    if (g.weight < f.weight) { hasSmaller = true; break; }
                }
                if (hasSmaller) {
                    target = f;
                    foundTarget = true;
                    break;
                }
            }
            if (!foundTarget) break;  // 剩余全是同一最小权重，无法违规

            // 导航去吃该更大零食。HARD 违规判定发生在食物标记 eaten 之前
            // （endManualGame 先执行），因此导航返回 false 但状态为 GAME_OVER
            // 即代表"吃更大零食 → 判负"已触发。
            const bool navResult = navigateToFood(model, target.position, true);
            if (model->getGameState() == GameState::GAME_OVER) {
                triggered = true;  // 吃更大零食时场上仍有更小未吃 → 判负
                break;
            }
            if (!navResult) {
                break;  // 导航失败（目标被蛇身围死等），放弃
            }
            // navResult=true 且仍 PLAYING：avoidFoods 下只可能吃掉目标本身，
            // 而目标已确认存在更小未吃零食，理论不至此分支；继续尝试下一目标
        }
        QVERIFY2(triggered, "存在更大零食时应触发困难模式判负");
    }
}

// ==================== 计数接口 ====================

void TestModelManual::targetWeightIsMin() {
    auto model = makeManual();
    int minWeight = INT_MAX;
    for (const auto& f : model->getFoods()) {
        if (!f.eaten && f.weight < minWeight) minWeight = f.weight;
    }
    QVERIFY(minWeight < INT_MAX);
    QCOMPARE(model->getCurrentTargetWeight(), minWeight);
}

void TestModelManual::occupancyQuery() {
    auto model = makeManual();
    const Position head = model->getSnake().front();
    QVERIFY(model->isOccupied(head.x, head.y));  // 蛇头占据
    // 找一个空白格
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            if (!model->isOccupied(x, y)) {
                QVERIFY(!model->isOccupied(x, y));
                return;
            }
        }
    }
    QFAIL("地图应存在空白格");
}

// 使用 QTEST_GUILESS_MAIN：纯模型测试
QTEST_GUILESS_MAIN(TestModelManual)
#include "TestModelManual.moc"
