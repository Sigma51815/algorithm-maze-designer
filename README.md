# 算法驱动的迷宫设计

> C++17 / Qt 6 Widgets / CMake / 跨平台（macOS / Windows / Linux）  
> 算法课设 — 迷宫设计组

迷宫探险桌面应用。四种算法生成完美迷宫，DP 求最优资源路径，分支限界求解 BOSS 战，遗传算法优化迷宫难度。

---

## 快速开始

### macOS / Linux

```bash
make            # 构建
make test       # 构建 + 28 个自测
make run        # 构建 + 启动 GUI
```

### Windows

```cmd
build.bat               # 构建 (Release)
build.bat test          # 构建 + 自测
build.bat run           # 构建 + 启动 GUI
```

**前提**: CMake 3.16+、Qt 6.x (Widgets)、VS 2019+ (Windows) / Clang/GCC

---

## 功能

| 模块 | 实现 |
|------|------|
| 迷宫生成 | 分治 / Kruskal MST / 回溯 DFS / 分支限界 BFS |
| 资源 DP | 树形 DP：金币 +50 / 陷阱 -30，只计首次访问 |
| BOSS 战 | 分支限界：乐观下界剪枝 + 状态记忆 + 边界压榨策略 |
| 贪心 AI | 3×3 局部视野，4 种策略 (性价比/最近/避开陷阱/趋近终点) |
| RL 玩家 | 表格型 Q-Learning（1M 状态，32MB Q 表） |
| GA 优化 | 种群进化：适应度 = regret × 拓扑难度因子 |
| 协同进化 | GA 迷宫种群 vs RL AI 种群交替进化 |
| 战斗动画 | 回合制动画窗口：血条/伤害特效/冷却/自动播放 |
| 导出 | 交叉测试 JSON（5 字段：maze/B/PlayerSkills/minRouds/CoinConsumption） |

---

## 源码结构

```
src/
  include/            ← 头文件
    maze.h            MazeModel（生成/DP/验证/JSON/拓扑）
    bosssolver.h      BossSolver（分支限界）
    mainwindow.h      MainWindow（GUI + 线程）
    mazewidget.h      MazeWidget（渲染）
    battlewindow.h    BattleWindow（战斗动画）
    maze_optimizer.h  MazeOptimizer（GA）
    maze_evaluator.h  MazeEvaluator（适应度）
    resource_placer.h ResourcePlacer（资源分布）
    coevolution.h     CoEvolution（协同进化）
    ai/               贪心/RL/AI格式
  source/             ← 实现文件
standalone_cpp_submissions/   ← 纯 C++ 控制台提交版
```

---

## 命令行

```bash
./build/maze_designer --self-test          # 28 个自测
./build/maze_designer --gui-smoke-test     # GUI 烟雾测试
./build/maze_designer --run-optimizer      # 无头优化器
```

---

## 文档

| 文档 | 说明 |
|------|------|
| [`docs/PROJ_GUIDE.md`](docs/PROJ_GUIDE.md) | **项目交接指南** — 筛查/检查/强化方向 |
| [`docs/PROJECT_WORKFLOW.md`](docs/PROJECT_WORKFLOW.md) | **项目运转流程** — 架构/线程/数据流全解 |
| [`docs/优化策略说明.md`](docs/优化策略说明.md) | **优化策略** — GA/适应度/协同进化 |
| [`docs/任务设计书.md`](docs/任务设计书.md) | 课设任务规范 |
| [`docs/课程设计说明.md`](docs/课程设计说明.md) | 算法原理与复杂度分析 |
| [`docs/验收对照清单.md`](docs/验收对照清单.md) | 助教验收对照项 |
| [`docs/AI玩家接口说明.md`](docs/AI玩家接口说明.md) | 交叉测试 JSON 接口 |
