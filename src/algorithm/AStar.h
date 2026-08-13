/**
 * @file AStar.h
 * @brief A*寻路算法模块
 *
 * 使用A*算法在网格地图上计算从起点到终点的最短路径。
 * 开放列表使用std::priority_queue（最小堆），关闭列表使用二维布尔数组。
 * 启发式函数采用曼哈顿距离。
 */

#ifndef ASTAR_H
#define ASTAR_H

#include <vector>
#include <utility>
#include <cstdint>

/**
 * @brief A*寻路算法命名空间
 */
namespace AStar {

/**
 * @brief 使用A*算法计算从起点到终点的最短路径
 *
 * @param startX      起点X坐标（列号，范围[0, gridSize)）
 * @param startY      起点Y坐标（行号，范围[0, gridSize)）
 * @param goalX       终点X坐标（列号，范围[0, gridSize)）
 * @param goalY       终点Y坐标（行号，范围[0, gridSize)）
 * @param obstacles   二维布尔数组，true表示该格子被障碍物占据（蛇身）
 * @param gridSize    网格大小（正方形网格的边长）
 * @return 路径坐标序列（pair<x,y>），包含起点，不包含终点。
 *         如果无可达路径，返回空vector。
 */
std::vector<std::pair<int, int>> findPath(
    int startX, int startY,
    int goalX, int goalY,
    const std::vector<std::vector<bool>>& obstacles,
    int gridSize
);

/**
 * @brief 使用A*算法计算从起点到终点的最短路径（扁平化障碍网格版本）
 *
 * 缓存友好变体：调用方直接提供行主序（row-major）扁平障碍数组
 * obstacleFlat[idx]（idx = y * gridSize + x），非零表示障碍。
 * 省去 vector<vector<bool>> 中间层：零堆分配转换、单次连续内存访问。
 * 语义与二维版本完全一致。
 *
 * @param obstacleFlat 行主序扁平障碍数组，长度 >= gridSize*gridSize，非零=障碍
 */
std::vector<std::pair<int, int>> findPath(
    int startX, int startY,
    int goalX, int goalY,
    const uint8_t* obstacleFlat,
    int gridSize
);

} // namespace AStar

#endif // ASTAR_H
