# QA 三轮审查 — 最终交接报告

> **目标**：给下一个 AI Agent 或开发者提供完整的修复记录 + 待处理问题清单  
> **分支**: `tmchao7-maze-dev` | **日期**: 2026-06-16  
> **审查方式**: 13 个并行 Explore Agent，三轮逐步深入

---

## 一、已修复问题（三轮共 14 项）

### 第一轮：线程安全 + 配置修复（5 项）

| # | 严重度 | 文件 | 问题 | 修复方式 |
|---|--------|------|------|---------|
| 1 | 🔴 | mainwindow.cpp:54-63 | `stopOptimizer()` 不调用 `optimizer_->stop()`，析构函数挂起 | 加 `stop()` + `disconnect(optimizer_, ...)` |
| 2 | 🔴 | mainwindow.cpp:518 | `generateMaze()` 不停止旧 GA/AI 线程 → 覆写新迷宫 | 首行加 `stopOptimizer()` + `stopAiWorker()` |
| 3 | 🔴 | mainwindow.cpp:951,998,821,847 | 优化/AI 期间按钮不禁用 → 可重复点击 | 启动时 `setEnabled(false)`，完成时恢复 |
| 4 | 🔴 | mainwindow.h + .cpp | `finished` 回调可能陈旧（race condition）| 加 `generationId_` 计数器 + 守卫 |
| 5 | 🟡 | main.cpp:844 | 无头优化器 `useSmartPlacement` 未显式关闭 | 加 `cfg.useSmartPlacement = false;` |

### 第二轮：内存泄漏 + 遗漏修复（5 项）

| # | 严重度 | 文件 | 问题 | 修复方式 |
|---|--------|------|------|---------|
| H1 | 🔴 | mainwindow.cpp:43-75, 858-915, 1020-1120 | `deleteLater()` 在 `wait()` 后调用 → 事件投递到已停止的事件循环 → 内存泄漏 | 改为：在 `quit()` 前调 `deleteLater()`，或 started lambda 中预删除 |
| H2 | 🔴 | mainwindow.cpp:783-786 | `loadMaze()` 漏了 `stopOptimizer()` | 添加 `stopOptimizer();` |
| H3 | 🔴 | mainwindow.cpp:43-52 | `stopAiWorker()` 不恢复 `generateButton_` → 按钮永久灰掉（GA 未勾选时）| 末尾添加 `generateButton_->setEnabled(true)` |
| M1 | 🟡 | mainwindow.cpp:669-684 | Boss 结果格式字符串缺 `%4`，字段错位 | 补上 `%4`（DP 最大金币）并重编号 |
| M2 | 🟡 | mainwindow.cpp:1022-1027, 859-864 | 陈旧回调守卫 return 时未置空 `optimizer_`/`optimizerThread_` 等指针 | 守卫中也置空指针 |

### 第三轮：计算逻辑 + 系统稳定性修复（3 项）

| # | 严重度 | 文件 | 问题 | 修复方式 |
|---|--------|------|------|---------|
| BUG1 | 🔴 | maze.cpp:438-452 | `placeResources()` 金币放置从未 shuffle，确定性偏袒分支单元格 | 加 `std::shuffle(coinCandidates)` |
| SS1 | 🟡 | mainwindow.cpp:1038-1044 | GA 完成回调替换 `maze_` 时未停止 `generationTimer_` → 定时器访问错误迷宫的边 | 添加 `generationTimer_->stop()` |
| SS2 | 🟡 | mainwindow.cpp:530-563, 796-818 | `generateMaze()` 和 `loadMaze()` 未清除旧 GA 状态（`hasOptimizedMaze_`、对比表、保存按钮）| 添加状态清除代码 |

---

## 二、待处理问题清单（给下一个 Agent）

### 🔴 HIGH — 计算逻辑严重缺陷

#### RL-1: Q-Learning ε 衰减太慢，2000 episodes 后 ε 仍 ~0.18
**文件**: `src/source/ai/rl_player.cpp` lines 237-238
- ε 从 0.5 开始，每 episode 乘以 0.9995
- 2000 episodes 后 ε ≈ 0.5 × 0.9995²⁰⁰⁰ ≈ 0.184
- 最小值设了 0.03，但从未到达
- **修复方向**: 增大 decay 因子或减小 episode 数

#### RL-2: Q-Learning ε 每个迷宫重置为 0.5
**文件**: `src/source/ai/rl_player.cpp` line 171
- `trainOnMazes()` 对每个迷宫调 `trainOnMaze()`，每次重置 ε = 0.5
- 过度探索，前一个迷宫学到的 Q 值被后续探索覆盖
- **修复方向**: 跨迷宫保持 ε 或只在第一批 maze 后衰减

#### RL-3: 距离奖励产生振荡激励
**文件**: `src/source/ai/rl_player.cpp` lines 207-210
- 走近终点 +0.01，走远不惩罚
- 从 A 走到 B（远离），再走回 A（接近），净赚 +0.01
- 步数惩罚仅 -0.002，不足以阻止振荡
- **修复方向**: 使用终点距离的绝对差值而非相对变化

#### RL-4: play() 中 ε=0 时可能死循环
**文件**: `src/source/ai/rl_player.cpp` lines 267, 274-284
- 纯贪心模式（ε=0）下，若 Q 表推荐无效动作（撞墙），agent 原地不动
- 状态不变 → 贪心重新选择 → 再次选同一无效动作 → 死循环直到 maxSteps
- **修复方向**: 加动作有效性检查或 ε-greedy 兜底

#### CE-1: CoEvolution `bestFitness` 初始化为 0，负适应度永远不被记录
**文件**: `src/include/coevolution.h` line 32, `src/source/coevolution.cpp` line 111
- `bestFitness = 0`，条件 `gaBest > result.bestFitness`
- 如果 RL 表现优于 DP（`dpVal - rl.totalResource ≤ 0`），bestFitness 永远是 0，不是实际最大值
- **修复方向**: 初始化为 `std::numeric_limits<int>::min()`

---

### 🟡 MEDIUM — 影响功能但不致崩溃

#### GP-1: GreedyPlayer 硬编码 50/-30 值做决策，但用实际值收集
**文件**: `src/source/ai/greedy_player.cpp` lines 107-109, 187
- 可见性决策用 `ch == 'G'` → +50, `ch == 'T'` → -30
- 实际收集用 `maze.resourceAt(pos)` 返回的真实值
- 若模型有非标准值（如 +100 金币），AI 按 +50 决策
- **修复方向**: 统一从模型读取值

#### GP-2: ValuePerStep 策略名不副实
**文件**: `src/source/ai/greedy_player.cpp` line 111
- 所有可见邻居的 `dist` 硬编码为 2
- `ValuePerStep = value / 2` → 退化为纯值排序
- BFS 实际距离从未参与计算
- **修复方向**: 用 BFS 计算每个可见候选的真实距离

#### GP-3: NearestFirst 策略等价于 "随机第一个硬币"
**文件**: `src/source/ai/greedy_player.cpp` lines 131-132, 141
- 所有可见硬币 score = 1.0/2 = 0.5
- 严格大于 (`>`) 打破平局 → 遍历顺序（上下左右）的第一个总是胜出
- **修复方向**: 用真实 BFS 距离，或 ≥ 打破平局

#### GP-4: AvoidTraps 兜底模式仍可走入陷阱
**文件**: `src/source/ai/greedy_player.cpp` lines 123, 148-158
- 当所有可见格都是陷阱（无硬币），`foundResource = false`
- 兜底逻辑从完整 `visible` 列表中选择访问次数最少的格 → 可能包含陷阱
- **修复方向**: 兜底也过滤陷阱

#### EC-1: `pathInefficiency` 无上界，可能主导适应度
**文件**: `src/source/maze_evaluator.cpp` lines 76-77, 108-111
- `ineff = (totalSteps - shortestPath) / shortestPath`，若 AI 走满 maxSteps 可达 ~100
- ×20 权重 → 贡献 2000，远超其他三项总和（~155）
- **修复方向**: clamp 到合理范围 [0, 5] 或 [0, 10]

#### EC-2: 拓扑难度项饱和后主导适应度
**文件**: `src/source/maze_evaluator.cpp` lines 108-111, 154
- `topoDifficulty` 上界 5.0，`topoWeight=0.3` → 贡献 75
- 此时 topo 项超越 coin(50) + trap(30) 之和
- `solutionLength/shortestPath ≥ 9` 就饱和（极易达到）
- **修复方向**: 降低 topoWeight 或缩放系数

#### EC-3: 跨策略独立取最大高估难度
**文件**: `src/source/maze_evaluator.cpp` lines 62-87
- coinMissRate / trapHitRate / pathInefficiency 可能来自不同策略
- 没有任何单一 AI 同时表现这么差
- **修复方向**: 对每个策略独立算 fitness，取最差的那个策略的完整 fitness

#### BS-1: `coinConsumption = max(1, dp.maxValue - 1)` 语义错误
**文件**: `src/source/bosssolver.cpp` line 213
- `dp.maxValue` 是净值（如 50 表示一个金币）
- 减 1 → 49，被导出为 `CoinConsumption`
- 当 `dp.maxValue=0` 时强制消费 1 金币，但玩家实际有 0 金币
- **修复方向**: 改为实际金币数 `dp.collectedCoins`（需要在 DP 结果中暴露此项）

---

### 🟢 LOW — 防御性改进

| # | 文件 | 问题 |
|---|------|------|
| L1 | maze_evaluator.cpp:46 | `shortestPathLen` 不可达终点时 fallback=1 → pathInefficiency 可能巨大 |
| L2 | maze.cpp:757-759 | DP maxValue 不排除 boss 单元格（实践无影响，资源从不放在 B）|
| L3 | bosssolver.cpp:47-73 | `optimisticTurnLowerBound()` 上界不紧 → 少剪枝但不错剪 |
| L4 | mazewidget.cpp:13-18 | `setMaze()` 不清 `aiPath_`（调用方都手动清了）|
| L5 | mazewidget.cpp:27-32 | `setAiPath`/`setSolutionPath` 无边界检查 |
| L6 | mainwindow.cpp:714 | BattleWindow 可重复打开多个 |
| L7 | maze_optimizer.h:49 | `setConfig()` 无输入验证（GUI 限制了范围）|
| L8 | main.cpp:929,941 | `QTimer::singleShot` 捕获裸指针（烟雾测试专用）|
| L9 | mainwindow.cpp:1112-1138 | `applyOptimizedMaze()` 死代码 — 从未被调用 |
| L10 | main.cpp:854-855 | 无头模式 `--rows`/`--cols` 无上限 clamp |
| L11 | rl_player.cpp:39-43 | `saveQTable()` 产生 ~4.19M 个 double 的 JSON（~200 MB），若被调用可能 OOM |
| L12 | rl_player.cpp:83-88 | 截断的 JSON 加载时未读取的 Q 表条目保留旧值 |
| L13 | rl_player.cpp:100-121 | 对角线邻居永远编码为"墙" → 浪费 256× 状态空间 |
| L14 | qlearning_optimizer.cpp:78 | `actionCount_=10` 静默截断墙候选，状态间 action 索引不对齐 |
| L15 | maze.cpp:777-883 | `expandedGrid()` vs `compactGrid()` S/E/B 标记优先级不一致 |

---

## 三、修改文件汇总

| 文件 | 三轮总改动 |
|------|-----------|
| `src/source/mainwindow.cpp` | +60 / -10（核心改动最多） |
| `src/include/mainwindow.h` | +1（`generationId_`） |
| `src/source/main.cpp` | +1（`useSmartPlacement = false`） |
| `src/source/maze.cpp` | +2（coin shuffle） |

---

## 四、验证状态

| 测试 | 结果 |
|------|------|
| `make test` (28 tests) | ✅ ALL TESTS PASSED |
| `--gui-smoke-test` | ✅ exit 0 |
| `--run-optimizer` | ✅ Valid: YES, Saved: optimizer_result.json |

---

## 五、后续 AI Agent 工作建议

### 优先级排序

1. **RL-3 + RL-4**（距离奖励振荡 + 死循环）— 影响 RL 自测稳定性
2. **CE-1**（CoEvolution bestFitness）— 影响协同进化结果正确性
3. **EC-1 + EC-2**（pathInefficiency 无界 + topo 主导）— 影响 GA 优化方向
4. **GP-1 ~ GP-4**（贪心策略准确性）— 影响评估结果可信度
5. **RL-1 + RL-2**（ε 衰减 + 重置）— RL 训练效果
6. **BS-1**（coinConsumption 语义）— 交叉测试 JSON 准确性
7. **L1 ~ L15** — 防御性改进

### 自测稳定性注意

- RL 自测偶尔 flaky（Q-Learning 随机初始化）
- 第一轮 GA comparison 测试中 `before` RL 分数波动大
- 新的 coin shuffle 使资源分布不再是严格确定性（但同 seed 仍可复现）
