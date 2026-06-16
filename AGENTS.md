# AGENTS.md

## 项目概述

算法课设 — 算法驱动的迷宫探险游戏。Qt 6 Widgets 桌面应用。

**核心任务（迷宫设计组）**：
1. 四种基础算法生成完美迷宫（分治 / Kruskal MST / DFS / BFS）
2. DP 求最优资源收集路径
3. 分支定界求解 BOSS 最优技能序列（自动计算限定回合数和复活金币）
4. **遗传算法（GA）迷宫难度优化**（PAIRED regret 最大化）
5. **GA + RL 协同进化**（CoEvolution，迷宫种群 vs AI 种群对抗）

**辅助功能（检验迷宫难度）**：
- 贪心 AI 玩家（3×3 局部视野，4 种策略）
- Q-Learning RL 玩家（表格型，1M 状态）
- 智能资源分布（金币放死胡同、陷阱放分叉口）
- 增强适应度评估（regret × 拓扑难度因子）

**技术栈**：C++17 / Qt 6 Widgets / CMake / 跨平台（macOS / Windows / Linux）

## 分支

- `main` — 稳定版本（组长 Windows 端）
- `tmchao7-maze-dev` — 当前开发分支（最新）

## 构建与运行

### macOS / Linux

```bash
make            # configure + build
make test       # build + run --self-test（28 个测试）
make run        # build + launch GUI
```

### Windows（原生）

```cmd
build.bat               # configure + build（Release）
build.bat debug         # Debug 构建
build.bat test          # build + run --self-test
build.bat run           # build + launch GUI
build.bat clean         # 删除 build 目录
```

前提：CMake 3.16+、Qt 6.x、Visual Studio 2019+（MSVC）

### 命令行运行（所有平台）

```bash
# macOS / Linux
./build/maze_designer --self-test
# Windows
.\build\Release\maze_designer.exe --self-test

# 参数说明
./build/maze_designer --gui-smoke-test      # GUI 烟雾测试
./build/maze_designer --run-optimizer       # 无头优化器（服务器用）
./build/maze_designer --run-optimizer --rows 7 --cols 7
```

## 源码结构

```
src/
  include/
    maze.h              # MazeModel（4算法生成、DP、验证、JSON、拓扑分析）
    bosssolver.h        # BossSolver（分支定界 + solveWithMaze 自动计算参数）
    mainwindow.h        # MainWindow（GUI + 线程管理）
    mazewidget.h        # MazeWidget（QPainter 渲染）
    battlewindow.h      # BattleWindow（BOSS 战斗动画）
    maze_optimizer.h    # MazeOptimizer（GA 优化器）+ OptimizerConfig/Stats
    maze_evaluator.h    # MazeEvaluator（增强适应度：regret × 拓扑难度）
    resource_placer.h   # ResourcePlacer（智能资源分布）
    maze_saver.h        # MazeSaver（JSON 序列化）
    qlearning_optimizer.h # QLearningOptimizer（Q-Learning 微调迷宫）
    coevolution.h       # CoEvolution（GA+RL 协同进化）
    ai/
      greedy_player.h   # GreedyPlayer（3×3 局部视野贪心）、PlayResult、GreedyStrategy
      rl_player.h       # RLPlayer（表格型 Q-Learning）、RLConfig、RLPlayResult
      aiplayerformat.h  # JSON contract builder（交叉测试格式）
  source/
    main.cpp            # 入口 + 28 自测 + GUI 启动 + --run-optimizer 无头模式
    maze.cpp            # MazeModel 实现
    bosssolver.cpp      # 分支定界 + solveWithMaze（边界压榨策略）
    mainwindow.cpp      # GUI 布局 + 线程安全清理
    mazewidget.cpp      # 迷宫渲染
    battlewindow.cpp    # 战斗动画
    maze_optimizer.cpp  # GA 优化器实现
    maze_evaluator.cpp  # 增强适应度评估
    resource_placer.cpp # 智能资源分布
    maze_saver.cpp      # JSON 序列化
    qlearning_optimizer.cpp # Q-Learning 微调
    coevolution.cpp     # 协同进化
    ai/
      greedy_player.cpp # 3×3 贪心实现
      rl_player.cpp     # Q-Learning 实现（堆分配 Q 表）
      aiplayerformat.cpp
```

## 关键类型

| 类型 | 文件 | 说明 |
|------|------|------|
| `MazeModel` | maze.h | 迷宫核心：passages_、resources_、generationSteps_、拓扑分析 |
| `MazeAlgorithm` | maze.h | 枚举：DivideAndConquer(0), KruskalMst(1), DepthFirstSearch(2), BreadthFirstSearch(3) |
| `ResourcePlan` | maze.h | DP 结果：maxValue, walk, collectedCells |
| `BossResult` | bosssolver.h | BOSS 基础结果：minimumTurns, skillSequence |
| `BossFullResult` | bosssolver.h | BOSS 完整结果：+roundLimit, coinConsumption, maxCoinsFromDP |
| `PlayResult` | greedy_player.h | 贪心 AI 结果：reachedEnd, score, walk, remainingResource |
| `RLPlayResult` | rl_player.h | RL 结果：totalResource, steps, path |
| `EvalResult` | maze_evaluator.h | 评估结果：dpScore, worstGreedyScore, regret, topoDifficulty, finalFitness |
| `OptimizerConfig` | maze_optimizer.h | GA 配置：含 useSmartPlacement, useEnhancedFitness, topoWeight |
| `CoEvolConfig` | coevolution.h | 协同进化配置 |
| `CellTopology` | maze.h | 拓扑分析：depth, branchDepth, onMainPath, isDeadEnd, isJunction |

## 核心算法

### 迷宫生成
- 四种算法均生成完美迷宫（生成树）。`validatePerfect()` 检查 E=V-1 + 连通性
- `MazeModel::statistics()` 返回路径长度、死胡同数、分叉口数、最长走廊

### 资源 DP
- `optimalResourceWalk()` — BFS 遍历 + DP，金币 +50 / 陷阱 -30，只计首次访问

### BOSS 战
- `BossSolver::solve()` — 分支定界 + 冷却感知下界剪枝
- `BossSolver::solveWithMaze()` — 自动计算限定回合数（最少+2）和复活金币（DP最优-1，边界压榨）

### 贪心 AI（3×3 视野）
- `GreedyPlayer::play()` — 只看周围 3×3 expanded grid，性价比策略，DFS 风格探索

### RL 玩家
- `RLPlayer` — 表格型 Q-Learning，状态编码 4 方向 × 4 种格子类型，Q 表 32MB 堆分配

### GA 优化器
- `MazeOptimizer::run()` — 种群进化，适应度 = regret × 拓扑难度
- 交叉：合并父代边集 → Union-Find 构建子代生成树
- 变异：30% 宏变异（重新生成），70% 微变异（边交换）
- 锦标赛选择 + 精英保留

### 智能资源分布
- `ResourcePlacer::placeSmart()` — 基于拓扑分析：金币放死胡同深处、陷阱放分叉口

### 增强适应度
- `MazeEvaluator::evaluate()` — regret(DP-Greedy) × (1 + topoWeight × topoDifficulty)
- 拓扑难度因子：死胡同密度、走廊长度、路径迂回度、分叉口密度

### 协同进化
- `CoEvolution::run()` — GA 迷宫种群 vs RL AI 种群交替进化

## 线程模型

- **GUI 主线程**：所有 UI 更新、信号处理
- **优化器线程**：`MazeOptimizer::run()` 在 QThread 上运行，8MB 栈
- **AI 玩家线程**：`GreedyPlayer::play()` 在 QThread 上运行，8MB 栈
- **清理**：`stopAiWorker()` 和 `stopOptimizer()` — disconnect → quit → wait → delete（不用 deleteLater）
- **stop 按钮**：DirectConnection + `std::atomic<bool> stopped_`

## 代码约定

- Qt 类型优先（QVector, QString, QJsonObject）— 除 std::array, std::mt19937, std::function, std::atomic
- `#pragma once` 头文件保护
- `Q_OBJECT` 宏在所有 QObject 派生类上
- 随机种子 `quint32` — 确定性可复现
- 新增 .h/.cpp 需更新 `CMakeLists.txt` 的 `add_executable()`
- JSON 导出：`MazeModel::toJson()`、`toCrossTestJson()`、`serializeAiPlayerInput()`

## 文档

| 文件 | 内容 |
|------|------|
| `docs/任务设计书.md` | 任务规范、评分标准 |
| `docs/方案甲_迷宫设计.md` | 迷宫设计方案 + PAIRED/GA 优化 + 边界压榨策略 |
| `docs/方案乙_AI玩家.md` | AI 玩家方案：贪心 / Q-Learning / DRQN |
| `docs/算法课程设计思路.md` | 整体设计思路、博弈论分析 |
| `docs/input说明.txt` | 交叉测试 JSON 格式规范 |
| `SERVER_GUIDE.md` | 服务器部署指南（无头优化器运行） |

## 已知问题

- RL 自测偶尔 flaky（~20% 失败率）— Q-Learning 随机初始化导致 before 分数波动大
- `minRouds` JSON key 拼写错误（应为 minRounds）— 保持兼容不改

## 后续发展方向

1. **微调**：RL 状态编码优化（当前对角线浪费 99.6% Q 表）、奖励函数调优
2. **Qt 可视化**：迷宫难度热力图、AI 路径对比面板、优化过程实时图表
3. **系统稳定性**：反复点击各种按钮的压力测试、长时间运行的内存泄漏检查
4. **Code simplifier**：资源放置模式提取为 helper（8 处重复）、mutate/edgeSwap 去重、crossover 去重
5. **注释补充**：核心算法添加中文注释
6. **原理讲解**：PAIRED 框架、边界压榨策略、协同进化的文档化
