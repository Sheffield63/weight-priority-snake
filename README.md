# 权重优先自动化贪吃蛇（SnakeGame）

基于 C++11 + Qt 5 的贪吃蛇桌面游戏：内置 **A* 寻路 AI 自动游玩** 与 **手动操控** 双模式，
支持三种玩法规则、三档速度、三套主题与本地排行榜。

![C++11](https://img.shields.io/badge/C%2B%2B-11-blue) ![Qt 5.15](https://img.shields.io/badge/Qt-5.15-green) ![CMake ≥ 3.16](https://img.shields.io/badge/CMake-%E2%89%A53.16-orange) ![CI](https://img.shields.io/github/actions/workflow/status/Sheffield63/weight-priority-snake/ci.yml?branch=master&label=CI) ![测试](https://img.shields.io/badge/tests-63%2F63%20passed-brightgreen) ![License MIT](https://img.shields.io/github/license/Sheffield63/weight-priority-snake)

---

## 功能特性

| 特性 | 说明 |
| --- | --- |
| 自动模式 | A* 寻路 + 权重优先目标选择，蛇自行吃零食并让位绕行；手动输入方向无效 |
| 手动模式 | 方向键 / WASD 控制，禁止 180° 掉头；随成长逐步加速（每吃 10 个快 15ms，下限 80ms） |
| 三种玩法规则 | 常见（零食不刷新）、正常（吃一个刷一个）、困难（必须从最小权重吃起，违规判负） |
| 三档速度 | 慢速 220ms / 中速 150ms / 快速 90ms，自动与手动模式均生效，跨局持久化 |
| 三套主题 | 浅色 / 深色 / 琥珀（QSS 皮肤），跨局持久化 |
| 音效 | 吃食 / 死亡 / 胜利 三种提示音，可开关，跨局持久化 |
| 排行榜 | 每局结束自动记录 Top10（得分 / 用时 / 吃数 / 时间），跨局持久化 |
| 暂停 | ESC 一键暂停 / 恢复 |
| 应用图标 | PE 资源图标 + 窗口 / 任务栏图标，已内嵌进可执行文件 |

## 操作说明

| 操作 | 按键 |
| --- | --- |
| 手动模式转向 | 方向键 或 W / A / S / D |
| 暂停 / 恢复 | ESC |

主界面按钮与下拉框：

- **开始 / 暂停 / 重置**：控制对局
- **模式**：自动（权重AI）· 手动 · 常见 · 手动 · 正常 · 手动 · 困难
- **速度**：慢速 · 220ms / 中速 · 150ms / 快速 · 90ms
- **主题**：浅色 · 深色 · 琥珀
- **音效开关**：开 / 关
- **排行榜**：查看历史 Top10

## 玩法规则

- **常见（NORMAL）**：场上零食数量固定，吃完不刷新。
- **正常（CLASSIC）**：每吃一个零食立即在随机空白处刷新一个，场上数量保持恒定。
- **困难（HARD）**：多个零食同时在场且权重不同（由正态分布生成，均值 3、最小 1、
  总和上限 624），必须始终吃**当前最小权重**的零食，
  若吃到更大权重零食而场上仍有更小未吃零食则直接判负。

分数 = 已吃零食的权重累计；撞墙、撞自身（蛇长 ≥ 4 时）、困难模式违规均判负。

## 构建与测试

依赖：CMake ≥ 3.16、Qt 5.15（Core / Widgets / Multimedia / Test）、MinGW 兼容工具链。

```bash
# 配置 + 构建
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=D:/Qt/5.15.2/mingw81_64
cmake --build build -j8

# 运行测试（Core 40 例 + Manual 23 例 = 63 例）
ctest --test-dir build --output-on-failure
```

构建产物：`build/SnakeGame.exe`（应用）、`build/SnakeGameTestsCore.exe`、`build/SnakeGameTestsManual.exe`（测试）。

### 打包发布（dist）

```powershell
# 部署 Qt 运行时依赖到 dist（保留编译器运行时 dll）
# 注：Qt 5.15.2 mingw81_64 的 windeployqt 会把插件误判为 debug，
#     --release 会过滤掉全部插件导致部署失败，故用 --debug 规避（产物仍是 release）
windeployqt --debug --no-translations --no-system-d3d-compiler `
  --no-opengl-sw --no-angle dist\SnakeGame.exe
```

> 主题 QSS、音效、应用图标均已通过 `resources/resources.qrc` + `resources/app.rc` 内嵌进 exe，
> 发布目录**不需要**携带 `resources/` 文件夹。

## 项目结构

```
Snakes/
├── CMakeLists.txt              # 主构建：应用 + 双测试目标 + windres 图标链接
├── src/
│   ├── main.cpp                # 入口（QApplication + 窗口图标）
│   ├── model/SnakeGameModel.h/.cpp    # 游戏核心：状态机 / 规则 / 速度 / 排行榜
│   ├── algorithm/AStar.h/.cpp          # A* 寻路（蛇身阻挡 + 让位绕行）
│   ├── controller/SnakeGameController.h/.cpp  # 游戏循环 / 信号连接 / 音效 / 弹窗
│   └── view/
│       ├── SnakeGameView.h/.cpp        # 主窗口：绘制 / 控件 / 主题
│       ├── GameOverDialog.h/.cpp       # 结算弹窗（统计 + 再来一局）
│       ├── LeaderboardDialog.h/.cpp    # 排行榜弹窗
│       └── Theme.h                     # 主题枚举与持久化映射
├── tests/
│   ├── TestModelCore.cpp       # 模型核心逻辑 40 例
│   └── TestModelManual.cpp     # 手动模式玩法 23 例（含 BFS 导航辅助）
├── resources/
│   ├── resources.qrc           # QSS / 音效 / 图标 → Qt 资源（内嵌 exe）
│   ├── app.rc                  # PE 图标资源（windres 编译链接）
│   ├── app.ico                 # 应用图标（工具生成，7 种尺寸）
│   ├── styles.qss / styles_dark.qss / styles_amber.qss
│   └── sounds/                 # eat.wav / death.wav / win.wav
├── tools/make_icon.cpp         # 图标生成工具（一次性，绘制蛇形图标）
└── dist/                       # 发布目录（windeployqt 打包产物）
```

## 配置持久化（QSettings）

| 键 | 含义 |
| --- | --- |
| `game/bestScore` | 历史最高分 |
| `settings/speedLevel` | 速度档位 0/1/2 |
| `settings/theme` | 主题 0/1/2（浅色/深色/琥珀） |
| `settings/soundEnabled` | 音效开关 |
| `leaderboard/entries` | 排行榜 Top10 记录 |

配置存储在 Windows 注册表（`HKEY_CURRENT_USER\Software` 下由应用名划分）。
