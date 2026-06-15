# Algorithm Maze Designer — 上下文交接文档

> 生成时间：2026-06-15 | 分支：`tmchao7-maze-dev` | 最新 commit：`142eaaa`

---

## 一、项目概览

算法课设（算法驱动的迷宫探险游戏），Qt 6 Widgets 桌面应用。

**核心功能**：
- 四种基础算法生成完美迷宫（分治 / Kruskal MST / DFS / BFS）
- DP 求最优资源收集路径
- 分支定界求解 BOSS 最优技能序列
- 贪心 AI 玩家（4 种策略）
- Q-Learning RL 玩家
- 遗传算法（GA）迷宫难度优化（PAIRED regret 最大化）
- GA + RL 协同进化（CoEvolution）
- QThread 异步优化 + AI 运行
- BOSS 战斗动画可视化
- JSON 导出（交叉测试格式）

**技术栈**：C++17 / Qt 6 Widgets / CMake + Ninja / macOS

---

## 二、源码结构

```
src/
  include/
    maze.h              # MazeModel（4算法生成、DP、验证、JSON）
    bosssolver.h        # BossSolver（分支定界）
    mainwindow.h        # MainWindow（GUI）
    mazewidget.h        # MazeWidget（QPainter 渲染）
    battlewindow.h      # BattleWindow（BOSS 战斗动画）
    maze_optimizer.h    # MazeOptimizer（GA 优化）+ OptimizerConfig/Stats
    maze_saver.h        # MazeSaver（JSON 序列化）
    qlearning_optimizer.h # QLearningOptimizer（Q-Learning 微调迷宫）
    coevolution.h       # CoEvolution（GA+RL 协同进化）
    ai/
      greedy_player.h   # GreedyPlayer, PlayResult, GreedyStrategy
      rl_player.h       # RLPlayer, RLConfig, RLPlayResult
      aiplayerformat.h  # JSON contract builder
  source/
    main.cpp            # 入口 + 20+ 自测（--self-test）+ GUI 启动
    maze.cpp            # MazeModel 实现
    bosssolver.cpp      # 分支定界
    mainwindow.cpp      # GUI 布局 + 所有业务逻辑
    mazewidget.cpp      # 迷宫渲染
    battlewindow.cpp    # 战斗动画
    maze_optimizer.cpp  # GA 优化器实现
    maze_saver.cpp      # JSON 序列化
    qlearning_optimizer.cpp # Q-Learning 微调
    coevolution.cpp     # 协同进化
    ai/
      greedy_player.cpp
      rl_player.cpp
      aiplayerformat.cpp
```

---

## 三、关键类型

| 类型 | 文件 | 说明 |
|------|------|------|
| `MazeModel` | maze.h | 迷宫核心：`passages_`（`QVector<array<bool,4>>`）、`resources_`、`generationSteps_` |
| `MazeAlgorithm` | maze.h | 枚举：`DivideAndConquer(0)`, `KruskalMst(1)`, `DepthFirstSearch(2)`, `BreadthFirstSearch(3)` |
| `ResourcePlan` | maze.h | DP 结果：`maxValue`, `walk`, `collectedCells` |
| `BossResult` | bosssolver.h | BOSS 战结果：`minimumTurns`, `skillSequence` |
| `PlayResult` | greedy_player.h | AI 结果：`reachedEnd`, `score`, `walk`, `totalSteps` |
| `OptimizerConfig` | maze_optimizer.h | GA 配置：含 `useMixedAlgorithms=true`（四算法混合） |
| `CoEvolConfig` | coevolution.h | 协同进化配置：含 `useMixedAlgorithms=true` |
| `RLConfig` | rl_player.h | RL 配置 |
| `MazeEdge` | maze.h | 边结构：`from`, `to` |

---

## 四、已完成功能与改动

### 4.1 四种基础算法（原有，未改动）

- `MazeModel::generate(rows, columns, algorithm, seed)` — switch 分发到四种算法
- `validatePerfect()` — 检查 E=V-1 + 连通性
- `optimalResourceWalk()` — DP 求最优资源路径
- `BossSolver::solve()` — 分支定界求最少回合

### 4.2 GA 优化器 — 四算法混合（commit `142eaaa`）

**核心改动**：GA 初始种群从「只用单一 `baseAlgorithm`」→「四种基础算法均匀轮转」。

| 文件 | 改动 |
|------|------|
| `maze_optimizer.h` | `OptimizerConfig` 新增 `bool useMixedAlgorithms = true;` + `static algorithmForIndex(int)` 声明 |
| `maze_optimizer.cpp` | `algorithmForIndex(i)` 实现（`i%4` 轮转四算法）；`randomChromosome(index, seed)` 按 index 选算法；`mutate()` 30% 宏变异随机选四算法之一 |
| `coevolution.h` | `CoEvolConfig` 新增 `bool useMixedAlgorithms = true;` |
| `coevolution.cpp` | 初始种群四算法轮转 |
| `main.cpp` | 新增自测：验证 `algorithmForIndex` 覆盖 4 种算法 + 混合 GA 产出合法迷宫 |

**设计意图**：交叉/微变异是边操作（算法无关），以前因父代同源无法融合不同算法结构特征。现在初始种群混合四算法，交叉能真正融合不同算法的结构特性（DFS 长走廊 + BFS 短分支等）。

### 4.3 GUI — GA 总开关 + 优化面板合并进①（commit `142eaaa` + 后续）

| 文件 | 改动 |
|------|------|
| `mainwindow.h` | 删除 `optAlgoBox_`；新增 `optEnableCheck_`、`optAlgoLabel_`、`aiWorkerThread_`、`aiStatusLabel_` |
| `mainwindow.cpp` | **UI 重组**：优化面板从独立 `⑤` GroupBox 合并进 `① 多方法生成完美迷宫` 内部（sub-separator 分隔） |

当前 GUI 布局：
```
① 多方法生成完美迷宫
  ├─ 算法选择 / rows / cols / seed / speed
  ├─ 生成按钮 + 验证标签 + 复杂度标签
  ├─ ── sub-separator ──
  ├─ ☐ 启用遗传算法优化（默认不勾，保护 CPU）
  │   ├─ 种群 / 代数 / 变异率
  │   ├─ 初始种群：四种算法均匀混合（info label）
  │   ├─ ☐ Q-Learning 精调 + 回合 / Top-K
  │   ├─ 运行 / 停止 / 应用 / 保存
  │   └─ progress / result labels
── separator ──
② 动态规划资源收集（coin/trap + DP 求解）
── separator ──
③ 分支限界 BOSS 战（health/skills/turns + 求解 + 战斗动画）
── separator ──
AI 玩家探险（运行贪心 AI + ⏳ loading 标签）
── separator ──
导出 AI 玩家 JSON
```

### 4.4 AI 玩家 QThread 异步

**修复**：`runAiPlayer()` 从主线程同步 → QThread 异步。
- 运行时显示 `⏳ AI 正在运行...` loading 标签
- 失败时显示 `❌ AI 未能到达终点`
- 成功后播放路径动画（`aiPathTimer_`）

### 4.5 `applyOptimizedMaze()` 状态重置

完整重置：停止所有 timer + AI worker、重置 `lastAiResult_`/`revealedPathPoints_`/`revealedAiPoints_`、隐藏 loading 标签。

---

## 五、构建与测试

```bash
make            # configure + build
make test       # build + run --self-test（20+ 检查）
make run        # build + launch GUI

./build/maze_designer --self-test        # 无头测试
./build/maze_designer --gui-smoke-test   # 窗口 200ms 后退出
```

**自测覆盖**：
- 4 种算法完美迷宫生成 + 连通性验证
- DP 最优性（4 算法 × 暴力对比）
- BFS 质量（5 个 seed × 路径长度 + 墙壁连续性）
- BOSS 战最优性（分支定界 vs 暴力 BFS）
- AI JSON 契约（字段顺序 + 15×15 矩阵格式）
- 贪心 AI 运行
- 3×3 局部测试
- BOSS 战斗
- GA 优化器（base regret vs optimized regret）
- RL 玩家（训练前后分数对比）
- 协同进化（2 cycles）
- **四算法混合 GA**（`algorithmForIndex` 覆盖 + 混合模式产出合法迷宫 + legacy 单算法兼容）

---

## 六、已知问题

### RL 自测 Flaky

`FAIL RL score decreased` — 在 baseline 代码上同样 flaky（已用 `git stash` 验证）。根因：2000 episode 的 Q-Learning 无法保证单调提升。与所有 GA/混合算法改动无关，未触及 `RLPlayer` 代码。

### 主题样式

GLM 模型在 `mainwindow.cpp` 的 `setStyleSheet` 中将暗色主题改为暖色主题（`#FFF8F0` 等）。这是独立的 UI 改动，与功能逻辑无关。

---

## 七、约束与注意事项

1. **本地是 MacBook Air，CPU 有限**：GA 优化和 RL 训练默认**关闭**（checkbox 默认不勾）
2. **Qt 类型优先**：使用 `QVector`/`QString`/`QJsonObject` 等 Qt 容器，不用 STL（除 `std::array`/`std::mt19937`/`std::function`）
3. **`#pragma once`** 头文件保护，无 include guards
4. **随机种子**：`quint32` 类型，确定性可复现
5. **新增 .h/.cpp 需更新 `CMakeLists.txt` 的 `add_executable()`**
6. **docs/ 目录被 gitignore**，只在本地存在

---

## 八、文档索引

| 文件 | 内容 |
|------|------|
| `docs/任务设计书.md` | 任务规范、评分标准 |
| `docs/方案甲_迷宫设计.md` | 迷宫设计方案 + PAIRED/GA 优化器（§五） |
| `docs/方案乙_AI玩家.md` | AI 玩家方案：贪心 / Q-Learning / DRQN |
| `docs/算法课程设计思路.md` | 整体设计思路、博弈论分析、五大范式整合 |
| `docs/input说明.txt` | 交叉测试 JSON 格式规范 |

---

## 九、Git 状态

```
分支：tmchao7-maze-dev（领先 origin 1 commit）
最新 commit：142eaaa feat: optimizer uses all four base algorithms; add GA master switch (default off)
未推送：是
未合并到 main：是
未跟踪：task_plan.md, CONTEXT_HANDOFF.md
```
