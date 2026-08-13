/**
 * @file AStar.cpp
 * @brief A*寻路算法实现（缓存优化版）
 *
 * 实现细节：
 * - 使用优先队列（std::priority_queue）作为开放列表，f值小的节点优先弹出
 * - 扁平化一维数组存储（idx = y * gridSize + x），替代嵌套 vector：
 *   消除 25 次独立堆分配，所有访问连续命中缓存
 * - gScore 数组记录每个节点的最优代价，仅当找到更优路径时才入队：
 *   消除重复节点入队，堆操作数量大幅下降
 * - parent 使用单 int 索引数组，路径回溯零跳转
 * - 启发式函数为曼哈顿距离：h = |x1-x2| + |y1-y2|
 */

#include "AStar.h"
#include <queue>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <functional>

namespace AStar {

/**
 * @brief A*搜索节点结构体（精简版，12字节，缓存友好）
 *
 * 相比原实现（24字节），移除了 x/y 冗余坐标和 h 字段：
 * - 坐标通过 idx = y*gridSize + x 推导
 * - h 通过 f - g 推导（比较器仅需 f/g）
 */
struct Node {
    int idx;  ///< 扁平化索引 = y * gridSize + x
    int g;    ///< 从起点到当前节点的实际代价
    int f;    ///< 总代价 f = g + h
};

/**
 * @brief 节点比较器 - 用于优先队列
 *
 * f值较小的节点优先级更高（最小堆）。
 * 当f值相同时，h值较小的优先（更接近目标）；h = f - g，即 g 较大者优先。
 */
struct NodeCompare {
    bool operator()(const Node& a, const Node& b) const {
        if (a.f != b.f) {
            return a.f > b.f; // 最小堆：f值小的优先
        }
        return a.g < b.g;    // f值相同时，g大（h小）的优先
    }
};

/**
 * @brief 计算两点之间的曼哈顿距离
 *
 * 曼哈顿距离 = |x1-x2| + |y1-y2|
 */
static int manhattanDistance(int x1, int y1, int x2, int y2) {
    return std::abs(x1 - x2) + std::abs(y1 - y2);
}

/**
 * @brief A*核心实现 - 直接消费扁平化障碍网格
 *
 * 障碍网格语义：obstacleFlat[idx] 非零 = 障碍（idx = y * gridSize + x，行主序）。
 * 所有状态数组（gScore/parent）均为扁平一维连续内存。
 */
static std::vector<std::pair<int, int>> findPathFlat(
    int startX, int startY,
    int goalX, int goalY,
    const uint8_t* obstacleFlat,
    int gridSize)
{
    // 存储最终路径的结果容器
    std::vector<std::pair<int, int>> path;

    // 检查起点和终点的有效性
    if (startX < 0 || startX >= gridSize || startY < 0 || startY >= gridSize ||
        goalX < 0 || goalX >= gridSize || goalY < 0 || goalY >= gridSize) {
        return path; // 坐标越界，返回空路径
    }

    // 检查终点是否被障碍物占据（不可达）
    const int goalIdx = goalY * gridSize + goalX;
    if (obstacleFlat[goalIdx]) {
        return path; // 终点是障碍物，返回空路径
    }

    // ===== 核心状态数组（全部扁平化一维，连续内存布局） =====
    const int cellCount = gridSize * gridSize;

    // gScore[idx]：起点到该节点的最优代价。INT_MAX 表示尚未访问。
    // 同时承担原实现 closed 列表的角色（gScore 已被更新 = 已入队过），
    // 且能识别"重复入队的劣质节点"（当前 g 大于已记录 gScore 则丢弃）。
    std::vector<int> gScore(static_cast<size_t>(cellCount), INT_MAX);

    // parent[idx]：父节点索引，-1 表示无父。单 int 替代 pair<int,int>，
    // 回溯时连续访问，无嵌套跳转。
    std::vector<int> parent(static_cast<size_t>(cellCount), -1);

    // ===== 开放列表 =====
    std::priority_queue<Node, std::vector<Node>, NodeCompare> openList;

    // ===== 四个移动方向：上、下、左、右 =====
    static const int dx[4] = {0, 0, -1, 1};
    static const int dy[4] = {-1, 1, 0, 0};

    // ===== 将起点加入开放列表 =====
    const int startIdx = startY * gridSize + startX;
    const int startH = manhattanDistance(startX, startY, goalX, goalY);

    gScore[startIdx] = 0;
    parent[startIdx] = startIdx; // 起点的父节点指向自身
    openList.push(Node{startIdx, 0, startH});

    // ===== A*主循环 =====
    while (!openList.empty()) {
        Node current = openList.top();
        openList.pop();

        // 跳过过期节点：该节点的 g 值已被更优路径更新（gScore 去重的关键）
        if (current.g != gScore[static_cast<size_t>(current.idx)]) {
            continue;
        }

        // ===== 到达终点，回溯路径 =====
        if (current.idx == goalIdx) {
            // 从终点回溯到起点（起点的父节点指向自身）
            int idx = goalIdx;
            while (idx != startIdx) {
                path.push_back({idx % gridSize, idx / gridSize});
                idx = parent[static_cast<size_t>(idx)];
            }

            // 加入起点
            path.push_back({startX, startY});

            // 反转路径（从起点到终点的顺序）
            std::reverse(path.begin(), path.end());

            return path;
        }

        // ===== 扩展相邻节点 =====
        const int cx = current.idx % gridSize;
        const int cy = current.idx / gridSize;
        const int baseG = current.g + 1; // 移动到邻居的代价（恒为 +1）

        for (int dir = 0; dir < 4; ++dir) {
            const int nx = cx + dx[dir];
            const int ny = cy + dy[dir];

            // 检查边界
            if (nx < 0 || nx >= gridSize || ny < 0 || ny >= gridSize) {
                continue;
            }

            const int nIdx = ny * gridSize + nx;
            const size_t nIdxSize = static_cast<size_t>(nIdx);

            // 检查是否为障碍物（扁平数组单字节读取）
            if (obstacleFlat[nIdxSize]) {
                continue;
            }

            // gScore 去重：仅当找到更优的 g 值时才入队。
            // 原实现无条件入队，同一节点最多可被入队 4 次；
            // 此检查将堆操作数量降至理论最小。
            if (baseG >= gScore[nIdxSize]) {
                continue;
            }

            // 更新最优代价与父节点
            gScore[nIdxSize] = baseG;
            parent[nIdxSize] = current.idx;

            // 计算启发式并入队
            const int newH = manhattanDistance(nx, ny, goalX, goalY);
            openList.push(Node{nIdx, baseG, baseG + newH});
        }
    }

    // 开放列表为空，终点不可达，返回空路径
    return path;
}

std::vector<std::pair<int, int>> findPath(
    int startX, int startY,
    int goalX, int goalY,
    const std::vector<std::vector<bool>>& obstacles,
    int gridSize)
{
    // ===== 扁平化障碍物网格（缓存友好转换，一次线性扫描） =====
    // uint8_t 而非 bool：vector<bool> 是位压缩容器，每次读写需移位+掩码；
    // 扁平 uint8_t 数组访问连续、无位操作开销。
    const int cellCount = gridSize * gridSize;
    std::vector<uint8_t> obstacleFlat(static_cast<size_t>(cellCount), 0);
    for (int y = 0; y < gridSize; ++y) {
        const std::vector<bool>& row = obstacles[y];
        const size_t rowBase = static_cast<size_t>(y) * gridSize;
        for (int x = 0; x < gridSize; ++x) {
            obstacleFlat[rowBase + x] = row[x] ? 1u : 0u;
        }
    }

    return findPathFlat(startX, startY, goalX, goalY, obstacleFlat.data(), gridSize);
}

std::vector<std::pair<int, int>> findPath(
    int startX, int startY,
    int goalX, int goalY,
    const uint8_t* obstacleFlat,
    int gridSize)
{
    return findPathFlat(startX, startY, goalX, goalY, obstacleFlat, gridSize);
}

} // namespace AStar
