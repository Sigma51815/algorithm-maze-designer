# QA 审查报告

> **审查依据**: [QA_CHECKLIST.md](./QA_CHECKLIST.md) | [BUG_ANALYSIS.md](./BUG_ANALYSIS.md)  
> **审查方式**: 7 个并行 Explore Agent 逐文件审查源代码  
> **分支**: `tmchao7-maze-dev` | **日期**: 2026-06-16

---

## 总览

| QA 章节 | 结果 | 发现项 |
|---------|------|--------|
| 一、四种基础算法生成迷宫 | ✅ PASS | 0 个问题 |
| 二、基础算法 + 优化开关 | ✅ PASS（附注）| 0 个严重问题 |
| 三、优化 vs 不优化对比指标 | ✅ PASS（附注）| 1 个小问题 |
| 四、程序崩溃测试 | ❌ FAIL | **3 个 HIGH 级问题** |
| 五、其他 Bug 检查 | ⚠️ MOSTLY PASS | 1 个中等 + 待验证项 |

**BUG_ANALYSIS.md 4 个 Bug 状态**：

| Bug | 状态 | 详情 |
|-----|------|------|
| #1: evaluateFitness 重新放置资源 | ✅ **已修复** | `skipPlacement = true` (maze_optimizer.cpp:142) |
| #2: 无头优化器 Smart Placement | ⚠️ **已确认** | 两标志同时为 true，需显式关闭 |
| #3: 适应度与 ai_score 脱节 | ⚠️ **部分修复** | 增强公式 OK，但 worstAIScore 仍未被使用 |
| #4: GUI 优化器配置 | ✅ **无问题** | 互斥逻辑正确 (mainwindow.cpp:967-968) |

---

## 一、四种基础算法生成迷宫 — ✅ PASS

### 1.1 功能验证

四个算法**全部正确产生完美迷宫（生成树）**：

| 算法 | 位置 (maze.cpp) | 机制 | 保证 |
|------|----------------|------|------|
| 分治 (Divide & Conquer) | 176–199 | 递归分割，每边界凿一个通道 | V−1 条边 |
| Kruskal MST | 202–222 | 并查集 + 随机边序 | V−1 条边 |
| DFS | 225–248 | 迭代栈，每个未访问邻居凿一条边 | V−1 条边 |
| BFS | 250–339 | 优先队列边界扩展，最多12次尝试，取最优 | V−1 条边 |

### 1.2 连通性验证

`validatePerfect()` (maze.cpp:455-497)：
- BFS 从起点计算可达单元格数 → 必须等于 cellCount()
- 遍历所有边求和/2 → 必须等于 cellCount() - 1
- 通过则显示绿色标签 `合法完美迷宫：V=N, E=N-1，连通且 E=V-1` (mainwindow.cpp:904)

### 1.3 动画验证

- 速度 50ms → 墙壁逐条出现 (generationTimer_ 触发)
- 速度 1ms → 瞬间完成
- 动画系统：`carve()` (maze.cpp:173) 每次追加边到 `generationSteps_`，MazeWidget 按 `revealCount_` 逐帧渲染

### 1.4 位置标记

`chooseDiameterEndpoints()` (maze.cpp:92-134)：
- S（起点）和 E（终点）设为迷宫直径两端（最远距离）
- B（BOSS）设为 E 在最短路径上的前一个单元格 (maze.cpp:131-133)
- 三段 BFS 计算，逻辑正确

### 1.5 尺寸与资源上限

资源自动上限 (maze.h:51-56)：
- 金币：`clamp(cellCount * 0.12, 3, 50)`
- 陷阱：`clamp(cellCount * 0.08, 2, 35)`
- 31×31 (15×15=225 cells): 27 金币 / 18 陷阱 → 低于上限

---

## 二、基础算法 + 优化开关 — ✅ PASS

### 2.1 算法一致性

GUI 优化器配置 (mainwindow.cpp:965)：`cfg.useMixedAlgorithms = false` — GUI 模式只用当前选中算法，不跨算法。✅

### 2.2 对抗模式 toggle

GUI 互斥逻辑 (mainwindow.cpp:967-968)：
```cpp
cfg.useAdversarialPlacement = optAdversarialCheck_->isChecked();
cfg.useSmartPlacement = !cfg.useAdversarialPlacement;
```
两者互斥。对抗模式资源由 ResourcePlacer 控制。

### 2.3 优化流程

- `preOptMaze_` 在 GA 启动前捕获快照 (mainwindow.cpp:970)
- 优化完成后自动显示对比表 (mainwindow.cpp:1029-1064)
- 对比使用相同评估配置 (`skipPlacement = true`, `topoWeight = 0.3`)

### ⚠️ 附注：无头优化器配置不一致

`main.cpp:844` 设置 `cfg.useAdversarialPlacement = true` 但**未显式设置** `cfg.useSmartPlacement = false`。由于 `OptimizerConfig` 结构体默认 `useSmartPlacement = true`，导致两个标志同时为 true。实际执行时 `if-else if` 链中 adversarial 优先，故实际行为正确，但配置语义不明确。**建议添加** `cfg.useSmartPlacement = false;`。

---

## 三、优化 vs 不优化对比指标 — ✅ PASS（附注 1 个小问题）

### 3.1 适应度公式验证

**实际代码** (maze_evaluator.cpp:107-111)：
```cpp
result.finalFitness = result.coinMissRate * 50.0
                    + result.trapHitRate * 30.0
                    + result.pathInefficiency * 20.0
                    + config.topoWeight * result.topoDifficulty * 50.0;
```

**QA_CHECKLIST 文档**：
```
fitness = coinMissRate × 50 + trapHitRate × 30 + pathInefficiency × 20 + topoWeight × topoDifficulty × 50
```

**结论：完全匹配。** ✅

### 3.2 对比评估一致性

对比表 (mainwindow.cpp:1031-1035)：
```cpp
EvaluatorConfig ec;
ec.skipPlacement = true;    // 不重新放置资源
ec.topoWeight = 0.3;
EvalResult before = MazeEvaluator::evaluate(preOptMaze_, ec);
EvalResult after = MazeEvaluator::evaluate(optimizedMaze_, ec);
```
"before" 和 "after" 使用相同配置，评估一致。✅

### 3.3 拓扑难度计算 (topoDifficulty)

(maze_evaluator.cpp:116-155)：

| 因子 | 公式 | 权重 |
|------|------|------|
| 死胡同密度 | `deadEnds / cellCount * 2.0` | ×2.0 |
| 走廊长度 | `min(1, longestCorridor/10) * 0.5` | ×0.5 |
| 路径迂回度 | `(solutionLength/shortestPath - 1) * 0.5` | ×0.5 |
| 分叉口密度 | `junctions / cellCount * 1.0` | ×1.0 |

结果 clamp 到 [0, 5]。✅

### ⚠️ 小问题：worstAIScore 未参与适应度

`EvalResult.worstAIScore` (= `remainingResource / totalSteps`) 在 maze_evaluator.cpp:84 被计算，但**从未在适应度公式中使用**。适应度公式仍是资源绝对值加权，不包含 AI 效率分数的归一化。这意味着：
- 短步数 + 低资源 的迷宫和 长步数 + 高资源 的迷宫适应度可能差异不反映用户可见的 AI Score。

**影响程度**：低 — 增强适应度公式通过 `pathInefficiency`（步数 vs 最短路径比）间接捕获了路径效率，但不完全是同一指标。

---

## 四、程序崩溃测试 — ❌ FAIL（3 个 HIGH 级问题）

### 🔴 HIGH-1：`stopOptimizer()` 不调用 `optimizer_->stop()`

**位置**：`mainwindow.cpp:54-63`

```cpp
void MainWindow::stopOptimizer() {
    if (!optimizerThread_) return;
    disconnect(optimizerThread_, nullptr, nullptr, nullptr);
    optimizerThread_->quit();     // ← 事件循环被阻塞，无法处理 quit
    optimizerThread_->wait();     // ← 阻塞直到 run() 返回
    optimizerThread_->deleteLater();
    ...
}
```

**问题**：
- `MazeOptimizer::stop()` (maze_optimizer.cpp:21-23) **从未被调用**
- GA 循环检查 `stopped_` (maze_optimizer.cpp:41): `for (int gen = 0; gen < config_.generations && !stopped_; ++gen)`
- 由于 `stopped_` 从未被设置为 true，GA 总是跑到完成
- `quit()` 投递事件给事件循环，但事件循环被 `run()` 阻塞，无法处理 quit
- **后果**：关闭窗口时，如果优化正在运行，析构函数会挂起直到 GA 自然完成

### 🔴 HIGH-2：优化运行中点击"生成"导致迷宫覆写

**位置**：`mainwindow.cpp:227-233, 518-548`

```cpp
// 生成按钮连接
connect(generateButton_, &QPushButton::clicked, this, [this]() {
    generateMaze();          // ← 先重新生成迷宫
    if (optEnableCheck_->isChecked() && maze_.cellCount() > 0) {
        preOptMaze_ = maze_;
        runOptimizer();      // ← guard: 已有 optimizerThread_ 则直接 return
    }
});
```

**问题链**：
1. `generateMaze()` 不调用 `stopOptimizer()`，不检查是否有正在运行的 GA
2. 新迷宫在主线程生成，覆盖 `maze_`
3. 旧 GA 线程继续运行，完成后触发 `finished` 回调 (mainwindow.cpp:998-1007)：
   ```cpp
   maze_ = optimizedMaze_;          // ← 用旧优化结果覆写新迷宫！
   mazeWidget_->setMaze(maze_);
   ```
4. **结果**：用户看到的是旧优化结果，不是刚生成的迷宫

### 🔴 HIGH-3：AI 运行中点击"生成"导致路径错位

**位置**：`mainwindow.cpp:518-522, 847-883`

```cpp
// generateMaze() 停止了 AI 动画定时器，但不停止 AI 工作线程
void MainWindow::generateMaze() {
    ...
    if (aiPathTimer_) aiPathTimer_->stop();   // line 521
    mazeWidget_->clearAiPath();               // line 522
    ...
}
```

当旧 AI 工作线程完成后，`finished` 回调 (line 847-883) 把旧迷宫的 AI 路径坐标绘制到新生成的迷宫上。如果新旧迷宫尺寸不同，`centerOf(aiPath_[i])` (mazewidget.cpp:129-131) 会产生错误的渲染位置。

### ⚠️ 中等：按钮在优化期间未禁用

`generateButton_` 在优化运行期间保持可点击。虽然有 `runOptimizer()` 的 guard (`if (optimizerThread_) return;`)，但 `generateMaze()` 仍会执行，触发 HIGH-2 的覆写 bug。

---

## 五、其他 Bug 检查 — ⚠️ MOSTLY PASS

### 5.1 UI 一致性

| 检查项 | 状态 |
|--------|------|
| 按钮文字正确 | ✅ |
| 优化完成后保存按钮启用 | ✅ (mainwindow.cpp:1003) |
| 优化完成后迷宫自动应用 | ✅ (mainwindow.cpp:1005-1006) |
| Boss 输入框宽 320px | 待 GUI 验证 |

### 5.2 DP 求解 & BOSS 战

**DP 求解**：`optimalResourceWalk()` (maze.cpp:676-763) — BFS 计算最短路径主干 + DFS 评估分支。分支净收益 ≤0 则跳过（`max(0, calculateGain(child))`），金币 +50 / 陷阱 -30，QSet 去重。✅

**BOSS 求解**：`BossSolver::solveWithMaze()` (bosssolver.cpp:195-216)：
- 回合限制 = 最少回合 + 2 (buffer turns, bossolver.h:39)
- 消耗金币 = `max(1, dpMaxValue - 1)`（"边界压榨"策略）
- `solve()` (bosssolver.cpp:78-178)：分支定界 DFS，贪心初始上界 + 乐观下界剪枝 + 状态记忆化

✅ 逻辑正确。

### 5.3 导出功能

**`toCrossTestJson()`** (maze.cpp:896-930) 导出全部 5 个字段：
- `maze` (line 910)
- `B` (line 916)
- `PlayerSkills` (line 925)
- `minRouds` (line 927) ⚠️ 拼写确认：应为 `minRounds`
- `CoinConsumption` (line 928)

`minRouds` 拼写错误是全系统一致的（3 处：maze.cpp:927, aiplayerformat.cpp:52, aiplayerformat.cpp:86），self-test 也针对拼错的 key 验证 (main.cpp:356)。这是已知可接受问题。

### 5.4 内存/性能

- 连续生成 10 次迷宫：待压力测试验证
- 连续优化 3 次：待压力测试验证

### 5.5 ResourcePlacer 详解

**三种放置策略** (resource_placer.cpp)：

| 策略 | 顺序 | 金币位置 | 陷阱位置 |
|------|------|---------|---------|
| `placeSmart()` (lines 3-91) | 金币 → 陷阱 | 50% 死胡同 → 深分支 → 随机 | 非主路径分叉口 → 主路径 → 随机 |
| `placeAdversarial()` (lines 93-173) | 陷阱 → 金币 | 深分支(branchDepth≥3) → 中深分支 → 随机，**排除死胡同** | 所有分叉口 70%（含主路径），无金币占用检查 |
| `placeRandom()` (lines 175-177) | 直接委托 `maze.placeResources()` | 随机 | 1/3 主路径，其余随机 |

关键差异：对抗模式**陷阱先放**，抢占最佳分叉口位置；**金币不放死胡同**，AI 更难找到。✅

### 5.6 QLearningOptimizer（独立模块）

`QLearningOptimizer` (qlearning_optimizer.cpp) 是一个**独立模块**，未与 GA 或任何其他组件集成：
- 状态编码：死胡同率 × 路径长度比 × 主路径陷阱率 → 75 个离散状态
- 动作：添加一堵墙（连通性保持的拓扑变异）
- 奖励：`regret(before) - regret(after)`（扩大贪心 vs 最优差距）
- 未被 `main.cpp`、`mainwindow.cpp`、`maze_optimizer.cpp`、`coevolution.cpp` 引用

这是一项预留的后续优化能力.

---

## BUG_ANALYSIS.md 对照审查

### Bug #1：evaluateFitness 重新放置资源 → ✅ **已修复**

**修复代码** (maze_optimizer.cpp:137-142)：
```cpp
// Use the same placement mode for the fitness formula, but
// skip actual re-placement (chromosome resources are already set
// by randomChromosome / mutate, matching config_.useAdversarialPlacement).
ec.useAdversarialPlacement = config_.useAdversarialPlacement;
ec.useSmartPlacement = false;
ec.skipPlacement = true;  // ← 跳过重新放置
```

`MazeEvaluator::evaluate()` (maze_evaluator.cpp:11-17) 检查 `skipPlacement`：
```cpp
if (!config.skipPlacement) {
    // ... placement logic (bypassed when skipPlacement=true)
}
```

**确认**：资源不再在评估时被覆盖。

### Bug #2：无头优化器用 Smart Placement → ⚠️ **已确认**

| 位置 | 代码 | 问题 |
|------|------|------|
| main.cpp:844 | `cfg.useAdversarialPlacement = true;` | 正确 |
| maze_optimizer.h:27 | `bool useSmartPlacement = true;` (默认) | **未显式覆盖为 false** |

**实际影响**：`if-else if` 链中 adversarial 优先，故实际使用对抗模式。但建议显式设置 `cfg.useSmartPlacement = false;` 以消除歧义。

### Bug #3：适应度与 AI Score 脱节 → ⚠️ **部分修复**

| 路径 | 代码 | 使用 worstAIScore？ |
|------|------|---------------------|
| 增强适应度 (useEnhancedFitness=true) | maze_evaluator.cpp:107-111 | ❌ 不使用 |
| 传统适应度 (useEnhancedFitness=false) | maze_optimizer.cpp:164 | ❌ 不使用 |

增强路径已改为复合公式（匹配 QA_CHECKLIST），通过 `pathInefficiency` 部分捕获步数效应，但 `worstAIScore` 仍未被直接优化。

### Bug #4：GUI 优化器配置 → ✅ **无问题**

GUI 配置 (mainwindow.cpp:967-968)：
```cpp
cfg.useAdversarialPlacement = optAdversarialCheck_->isChecked();
cfg.useSmartPlacement = !cfg.useAdversarialPlacement;
```
互斥逻辑正确。

---

## 修复建议（按优先级）

### 🔴 P0（必须修复 — 崩溃/数据损坏）

1. **`stopOptimizer()` 调用 `optimizer_->stop()`**
   ```cpp
   // mainwindow.cpp:54-63, 在 quit() 之前添加：
   if (optimizer_) optimizer_->stop();
   ```

2. **`stopAiWorker()` 调用 AI 停止逻辑**
   同上模式，在 quit() 之前通知 worker 停止。

3. **`generateMaze()` 中先停止优化器和 AI**
   ```cpp
   // mainwindow.cpp:518, 在生成新迷宫之前添加：
   stopOptimizer();
   stopAiWorker();
   ```

4. **优化运行中禁用生成按钮**
   ```cpp
   // runOptimizer() 开始时：
   generateButton_->setEnabled(false);
   // finished 回调中：
   generateButton_->setEnabled(true);
   ```

### 🟡 P1（建议修复 — 逻辑正确性）

5. **无头优化器显式设置 useSmartPlacement**
   ```cpp
   // main.cpp:844 之后添加：
   cfg.useSmartPlacement = false;
   ```

6. **优化完成回调中检查迷宫一致性**
   ```cpp
   // mainwindow.cpp:998, finished 回调中：
   // 检查 preOptMaze_ 是否仍然有效（未被新生成覆盖）
   ```

### 🟢 P2（优化改进）

7. **考虑在适应度公式中加入 worstAIScore**
   ```cpp
   // maze_evaluator.cpp:107-111 可添加：
   // + std::abs(result.worstAIScore) * someWeight
   ```

---

## 附录：审查覆盖的源文件

| 文件 | 审查 Agent | 关键发现 |
|------|-----------|---------|
| `src/source/maze.cpp` | Agent 5 | 四算法正确，DP 正确，validatePerfect 正确 |
| `src/source/maze_optimizer.cpp` | Agent 1 | Bug #1 已修复，crossover/mutate 正确重新放置资源 |
| `src/source/maze_evaluator.cpp` | Agent 3 | 公式匹配文档，topoDifficulty 计算正确 |
| `src/source/mainwindow.cpp` | Agent 4 | **3 个 HIGH 级线程安全问题** |
| `src/source/main.cpp` | Agent 2 | Bug #2 已确认，自测覆盖良好 |
| `src/source/ai/greedy_player.cpp` | Agent 6 | 线程安全，只读，4 策略均测试 |
| `src/source/ai/rl_player.cpp` | Agent 6 | 非线程安全但未在 GUI 线程中使用 |
| `src/source/coevolution.cpp` | Agent 6 | 资源种子确定性，无重新放置问题 |
| `src/source/resource_placer.cpp` | Agent 7 | 三种策略正确：Smart（金币优先，死胡同）、Adversarial（陷阱优先，分叉口）、Random |
| `src/source/maze_saver.cpp` | Agent 7 | 5 字段导出正确，`minRouds` 拼写全系一致 |
| `src/source/bosssolver.cpp` | Agent 7 | 分支定界正确，回合限制 = 最少+2，金币 = max(1,dp-1) |
| `src/source/qlearning_optimizer.cpp` | Agent 7 | 独立模块，75 状态 Q-Learning，未集成到任何流程 |
