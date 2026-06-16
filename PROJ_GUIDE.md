# PROJ_GUIDE — 项目交接指南（组长端 AI Agent 入口）

> **目标读者**: 组长端 AI Agent / 接手的开发者  
> **项目**: 算法驱动的迷宫探险游戏（算法课设）  
> **技术栈**: C++17 / Qt 6 Widgets / CMake / 跨平台  
> **当前分支**: `main`（稳定版，已合并所有修复）

---

## 一、项目概览

迷宫设计桌面应用，核心功能：

| 模块 | 说明 |
|------|------|
| 迷宫生成 | 4 种算法（分治/Kruskal/DFS/BFS）生成完美迷宫 |
| 资源 DP | BFS+DFS 分支评估，求最优金币收集路径 |
| BOSS 战 | 分支定界自动计算最优技能序列 + 战斗动画 |
| GA 优化 | 遗传算法优化迷宫难度（regret 最大化） |
| 协同进化 | GA 迷宫种群 vs RL AI 种群对抗 |
| AI 玩家 | 3×3 局部视野贪心（4 策略）+ Q-Learning RL |

---

## 二、快速开始

### macOS / Linux

```bash
make            # 配置 + 构建
make test       # 构建 + 运行 28 个自测
make run        # 构建 + 启动 GUI
```

### Windows（原生）

```cmd
build.bat               # 配置 + 构建（Release，MSVC）
build.bat test          # 构建 + 运行自测
build.bat run           # 构建 + 启动 GUI
build.bat clean         # 删除构建目录
```

**前提**: CMake 3.16+、Qt 6.x（Widgets 组件）、Visual Studio 2019+（Windows）或 Clang/GCC（macOS/Linux）

### 命令行模式

```bash
./build/maze_designer --self-test          # 无头自测
./build/maze_designer --gui-smoke-test     # GUI 烟雾测试
./build/maze_designer --run-optimizer      # 无头优化器
```

---

## 三、源码结构

```
src/
  include/
    maze.h              # MazeModel（4 算法、DP、验证、JSON）
    bosssolver.h        # BossSolver（分支定界）
    mainwindow.h        # MainWindow（GUI + 线程管理）
    mazewidget.h        # MazeWidget（QPainter 渲染）
    battlewindow.h      # BattleWindow（BOSS 战斗动画）
    maze_optimizer.h    # MazeOptimizer（GA）+ OptimizerConfig
    maze_evaluator.h    # MazeEvaluator（增强适应度：regret×拓扑难度）
    resource_placer.h   # ResourcePlacer（智能/对抗资源分布）
    maze_saver.h        # MazeSaver（JSON 序列化）
    coevolution.h       # CoEvolution（GA+RL 协同进化）
    ai/
      greedy_player.h   # GreedyPlayer（3×3 局部视野，4 策略）
      rl_player.h       # RLPlayer（表格型 Q-Learning）
      aiplayerformat.h  # JSON contract builder（交叉测试格式）
  source/               # 对应 .cpp 实现
```

---

## 四、已完成工作 / 三轮 QA 修复（14 项）

详见 `docs/QA_HANDOFF_REPORT.md`。主要修复：

### 线程安全（P0）
- `stopOptimizer()` 调用 `optimizer_->stop()` + 断开所有信号
- 生成新迷宫前自动停止旧 GA/AI 线程
- `generationId_` 陈旧回调守卫
- 按钮禁用/启用防止重复操作

### 内存管理
- `deleteLater()` 泄漏：改为在 `quit()` 前删除，worker thread 中预删除
- 陈旧回调守卫置空成员指针

### 计算逻辑
- 金币候选人随机打乱（消除分支偏袒）
- Boss 输出格式字符串补缺 `%4`
- GA 完成时停止生成动画定时器

### 构建系统
- CMake 跨平台：移除硬编码 `-G "Unix Makefiles"`
- 新增 `build.bat`（Windows 原生 MSVC 构建）
- `.gitignore` 增加 Windows/VS 构建产物

---

## 五、后续筛查方向

以下方向在当前代码中**已确认存在问题**，需下一轮审查/修复：

### 5.1 RL 玩家计算缺陷（HIGH）

| # | 问题 | 文件:行 | 说明 |
|---|------|---------|------|
| RL-1 | ε 衰减过慢 | `rl_player.cpp:237-238` | ε=0.9995，2000 episodes 后 ε≈0.18，远未到下限 0.03 |
| RL-2 | ε 每个迷宫重置 | `rl_player.cpp:171` | `trainOnMazes` 对每个迷宫重置 ε=0.5，前一个迷宫学到的 Q 值被覆盖 |
| RL-3 | 距离奖励振荡 | `rl_player.cpp:207-210` | 走近+0.01、走远不惩罚，agent 可靠振荡蹭奖 |
| RL-4 | 贪心死循环 | `rl_player.cpp:274-284` | ε=0 时选无效动作→状态不变→重复→直到 maxSteps |

### 5.2 适应度函数设计问题（MEDIUM）

| # | 问题 | 文件:行 | 说明 |
|---|------|---------|------|
| EC-1 | pathInefficiency 无上界 | `maze_evaluator.cpp:76-77` | AI 走满 maxSteps 时此项可达 ~2000，远超其他项 |
| EC-2 | topoDifficulty 饱和后主导 | `maze_evaluator.cpp:108-111` | solutionLength/shortestPath≥9 即饱和，topo 项(75)>coin(50)+trap(30) |
| EC-3 | 跨策略独立取最大 | `maze_evaluator.cpp:62-87` | coinMissRate/trapHitRate/pathInefficiency 可能来自不同策略 |

### 5.3 CoEvolution 问题（HIGH）

| # | 问题 | 文件:行 | 说明 |
|---|------|---------|------|
| CE-1 | bestFitness 初始化为 0 | `coevolution.h:32` `coevolution.cpp:111` | 负适应度永不记录 |

### 5.4 贪心 AI 准确性（MEDIUM）

| # | 问题 | 文件:行 | 说明 |
|---|------|---------|------|
| GP-1 | 决策值硬编码 | `greedy_player.cpp:107-109` | 用 50/-30 决策但实际收集用模型真实值 |
| GP-2 | ValuePerStep 名不副实 | `greedy_player.cpp:111` | dist 硬编码 2，退化为纯值排序 |
| GP-3 | NearestFirst 等价随机 | `greedy_player.cpp:131-132` | 所有硬币 score 相同，遍历顺序决定 |
| GP-4 | AvoidTraps 兜底可走入陷阱 | `greedy_player.cpp:148-158` | 无硬币可见时兜底不过滤陷阱 |

---

## 六、检查方向

### 6.1 自测稳定性
- RL 自测 ~20% 失败率（Q-Learning 随机初始化，RL-1~4 修复后可改善）
- 重跑 `make test` 即可通过

### 6.2 交叉测试兼容性
- JSON 导出 key `minRouds` 拼写为已知问题（应为 minRounds）
- `coinConsumption` 语义有误（见 `docs/QA_HANDOFF_REPORT.md` BS-1）

### 6.3 性能边界
- 大迷宫（50×50=2500 cells）：DP/BFS 仍 O(V)，安全
- RL Q 表 32MB 堆分配，`saveQTable` 会产生 ~200MB JSON（切勿调用）
- GA 种群×代数不宜同时设太大（MacBook Air 无散热）

### 6.4 Windows 兼容
- 代码零平台特定 API（纯 Qt6/C++17）
- `build.bat` 使用 MSVC + CMake，需 Qt6 安装在默认路径或设置 `CMAKE_PREFIX_PATH`
- 建议组长在 Windows 上跑一次 `build.bat test` 验证

---

## 七、强化方向

按优先级排列：

### 7.1 短期（可立即动手）
1. **修复 RL 训练**：调大 ε decay + 跨迷宫保持 ε + 改用绝对距离奖励
2. **clamp pathInefficiency**：限制到 [0, 5] 避免此项主导适应度
3. **降低 topoWeight**：从 0.3 降到 0.15 平衡各项贡献
4. **修复 CoEvolution bestFitness**：初始化为 `INT_MIN`
5. **修复贪心策略**：用 BFS 真实距离替代硬编码 dist=2

### 7.2 中期（功能增强）
6. **DRQN 玩家**：替换表格型 RL，用神经网络处理 3×3 视野
7. **迷宫难度热力图**：在 MazeWidget 上可视化各区域难度
8. **优化过程实时图表**：QChart 显示适应度/regret 随代际变化
9. **压力测试**：反复点击按钮的自动化测试

### 7.3 长期（质量提升）
10. **提取 helper**：减少 8 处资源放置代码重复
11. **mutate/edgeSwap 去重**：两个函数有相似逻辑
12. **注释补充**：核心算法添加中文注释
13. **PAIRED 框架文档化**：原理讲解 + 论文引用

---

## 八、关键文档索引

| 文档 | 路径 | 说明 |
|------|------|------|
| QA 检查清单 | `docs/QA_CHECKLIST.md` | 逐项验收标准 |
| QA 第一轮报告 | `docs/QA_REVIEW_REPORT.md` | 对照清单的首次审查 |
| QA 第二轮报告 | `docs/QA_REVIEW_ROUND2.md` | 已修复代码边缘 + UI/内存 |
| QA 最终交接报告 | `docs/QA_HANDOFF_REPORT.md` | 13 项已修复 + 30 项待处理 |
| Bug 分析 | `docs/BUG_ANALYSIS.md` | 原始 bug 分析（GA 优化后 AI 分数变高） |
| 任务设计书 | `docs/任务设计书.md` | 课设评分标准 |
| 迷宫设计方案 | `docs/方案设计思路/方案甲_迷宫设计.md` | PAIRED/GA 设计 |
| AI 玩家方案 | `docs/方案设计思路/方案乙_AI玩家.md` | 贪心/RL/DRQN 设计 |
| 优化器指南 | `docs/方案设计思路/OPTIMIZER_GUIDE.md` | 无头优化器使用 |
| 输入说明 | `docs/输入输出说明/input说明.txt` | 交叉测试 JSON 格式 |
| 验收手册 | `docs/输入输出说明/助教验收与评分操作手册.html` | 助教评分标准 |
