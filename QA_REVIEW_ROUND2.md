# QA 第二轮审查报告

> **审查方式**: 3 个并行 Explore Agent，分别审查：①已修复代码边缘情况 ②UI/Widget 文件 ③内存安全与边界条件  
> **分支**: `tmchao7-maze-dev` | **日期**: 2026-06-16

---

## 发现总览

| 严重度 | 数量 | 说明 |
|--------|------|------|
| 🔴 HIGH | 3 | 内存泄漏、loadMaze 遗漏 stopOptimizer、按钮永久禁用 |
| 🟡 MEDIUM | 2 | 格式字符串缺 %4、陈旧回调未空指针 |
| 🟢 LOW | 9 | 防御性缺失、输入验证、UX 细节 |

---

## 🔴 HIGH 级问题

### H1. `deleteLater()` 在 `wait()` 之后调用导致内存泄漏

**位置**: `mainwindow.cpp` — `stopAiWorker()` (line 50)、`stopOptimizer()` (line 65)、AI 完成回调 (line 900-901)、GA 完成回调 (line 1097-1098)

**根因**: 工作线程在 `quit()` + `wait()` 后事件循环已停止。`MazeOptimizer` 和 AI worker `QObject` 通过 `moveToThread()` 移到了工作线程。对这些对象调用 `deleteLater()` 会向已停止的事件循环投递 `QDeferredDeleteEvent`，该事件永远不会被处理。

```cpp
// stopOptimizer() 的当前代码：
optimizerThread_->quit();         // 事件循环退出
optimizerThread_->wait();         // 阻塞直到线程结束
optimizer_->deleteLater();        // ← 投递到已停止的事件循环 → 泄漏！
```

**影响**: 反复优化/跑 AI 会累积泄漏（每个 MazeOptimizer 包含完整迷宫和种群数据）。

**修复方向**: 在 `wait()` 之前调用 `deleteLater()`（Qt 标准写法），或在 `wait()` 之后直接用 `delete`。

### H2. `loadMaze()` 不调用 `stopOptimizer()`

**位置**: `mainwindow.cpp` lines 783-786

```cpp
void MainWindow::loadMaze() {  // line ~780
    generationTimer_->stop();
    pathTimer_->stop();
    aiPathTimer_->stop();
    stopAiWorker();          // ← 停了 AI，但没有 stopOptimizer()！
    ...
```

`generateMaze()` 已修复为调用 `stopOptimizer()` + `stopAiWorker()`，但 `loadMaze()` 遗漏了 `stopOptimizer()`。如果加载迷宫时 GA 正在运行，GA 完成后会覆写已加载的迷宫。

**修复**: 添加 `stopOptimizer();`。

### H3. `stopAiWorker()` 不恢复 `generateButton_` 启用状态

**位置**: `mainwindow.cpp` lines 43-52

`stopOptimizer()` 在 line 68 有 `generateButton_->setEnabled(true);`，但 `stopAiWorker()` 缺少此调用。

**触发路径**（当 GA 复选框未勾选时）:
- 用户点"运行贪心 AI" → `runAiPlayer()` 禁用按钮 (line 834)
- 用户点"生成" → `generateMaze()` → `stopAiWorker()` → **按钮保持禁用**
- GA 未勾选，`runOptimizer()` 不执行 → 按钮永远不会恢复

**修复**: 在 `stopAiWorker()` 末尾添加 `if (generateButton_) generateButton_->setEnabled(true);`。

---

## 🟡 MEDIUM 级问题

### M1. Boss 结果输出格式字符串缺少 `%4` 导致字段错位

**位置**: `mainwindow.cpp` lines 669-684 (`solveBossBattle()`)

格式字符串有 `%1, %2, %3, %5, %6, %7` — **缺少 `%4`**。Qt 的 `.arg()` 链式调用按序号递增匹配，导致参数错位：

| .arg() 调用 | 期望显示 | 实际替换的占位符 | 实际显示 |
|-------------|---------|----------------|---------|
| .arg(minimumTurns) | 最少回合数 | %1 | ✓ 正确 |
| .arg(roundLimit) | 限定回合数 | %2 | ✓ 正确 |
| .arg(coinConsumption) | 复活金币 | %3 | ✓ 正确 |
| .arg(maxCoinsFromDP) | — | **%5** | ✗ "最优序列"位置显示数字 |
| .arg(sequenceNames) | 最优序列（字符串）| **%6** | ✗ "搜索展开"位置显示技能名 |
| .arg(expandedStates) | 搜索展开数 | **%7** | ✗ "剪枝"位置显示展开数 |
| .arg(prunedStates) | 剪枝数 | *(无匹配)* | ✗ 丢失 |

**修复**: 补上 `%4` 或移除多余的 `.arg()` 并重新编号。

### M2. 陈旧回调守卫未置空成员指针

**位置**: `mainwindow.cpp` — GA finished 回调 (lines 1022-1027)、AI finished 回调 (lines 859-864)

当 `genAtStart != generationId_` 触发守卫时，回调执行：
```cpp
optimizer->deleteLater();
thread->deleteLater();
return;
// ← optimizer_ 和 optimizerThread_ 未置空！
```

如果之后 `stopOptimizer()` 被调用，会看到一个非空的 `optimizerThread_`，执行冗余的 stop/disconnect/quit/wait。虽然不会崩溃，但使 `runOptimizer()` 的 guard (`if (optimizerThread_) return;`) 在陈旧回调后错误地阻止新 GA 启动。

**修复**: 在守卫的提早返回路径中也置空 `optimizer_`/`optimizerThread_` 和 `aiWorker_`/`aiWorkerThread_`。

---

## 🟢 LOW 级问题（防御性/UX）

| # | 问题 | 位置 | 说明 |
|---|------|------|------|
| L1 | `setMaze()` 不清空 `aiPath_` | mazewidget.cpp:13-18 | 不对称：清 `solutionPath_` 但不清 `aiPath_`。调用方需要额外调用 `clearAiPath()` |
| L2 | `setAiPath`/`setSolutionPath` 无边界检查 | mazewidget.cpp:27-32, 40-45 | 若被提供旧迷宫的 cell 索引，渲染会错位（不崩溃） |
| L3 | 优化器/AI 回调中 `setMaze` 后于 `clearAiPath` | mainwindow.cpp:1034-1035, 1129-1130 | 调用顺序先 setMaze 再 clearAiPath，存在短暂窗口 |
| L4 | BattleWindow 可重复打开 | mainwindow.cpp:714 | 每次点"战斗动画"创建新窗口，无复用/限制 |
| L5 | `MazeOptimizer::setConfig()` 无输入验证 | maze_optimizer.h:49 | `populationSize=0` 会导致 UB，但 GUI 限制了最小值 |
| L6 | `tournamentSelect()` 在 `tournamentSize==0` 时空指针解引用 | maze_optimizer.cpp:380 | 默认值 3，未暴露给 UI，需代码级误用 |
| L7 | `coevolution.cpp` 在 `eliteCount > popSize` 时数组越界 | coevolution.cpp:84-87 | 需 `popSize < 5` 的异常配置 |
| L8 | `shortestPath = -1` 被静默接受 | maze_evaluator.cpp:144-146 | 不连通迷宫产生 `pathRatio=1.0`（无崩溃但结果无意义） |
| L9 | 死代码：`applyOptimizedMaze()` 从未被调用 | mainwindow.h:90, mainwindow.cpp:1112 | 已实现但未连接到任何信号/按钮 |

---

## 首轮修复验证

| 首轮 Fix | 状态 | 备注 |
|----------|------|------|
| Fix 1: `stopOptimizer()` 加 `stop()` + `disconnect` | ✅ 正确 | `stopped_` 竞态窗口是良性的（多跑一代） |
| Fix 2: `generateMaze()` 首行加 `stopOptimizer/AiWorker` | ⚠️ 遗漏 `loadMaze()` | H2 |
| Fix 3: 按钮禁用/启用 | ⚠️ `stopAiWorker()` 遗漏恢复 | H3 |
| Fix 4: `generationId_` 陈旧守卫 | ⚠️ 守卫未置空指针 | M2 |
| Fix 5: `useSmartPlacement = false` | ✅ 正确 | — |

---

## 修复优先级建议

### P0（本次修复）
1. **H1**: 修复 `deleteLater()` 泄漏 — 移 `deleteLater` 到 `wait()` 之前，或改用 `delete`
2. **H2**: `loadMaze()` 添加 `stopOptimizer()`
3. **H3**: `stopAiWorker()` 末尾添加 `generateButton_->setEnabled(true)`
4. **M2**: 陈旧守卫中置空 `optimizer_`/`optimizerThread_`/`aiWorker_`/`aiWorkerThread_`

### P1（可后续修复）
5. **M1**: Boss 格式字符串补 `%4`

### P2（可记录不改）
6. L1-L9: 防御性改进和 UX 优化
