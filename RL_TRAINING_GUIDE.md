# Q-Learning RL 训练开关说明

## 概述

迷宫优化器（MazeOptimizer）支持两种运行模式：

| 模式 | 开关 | 行为 | CPU 负载 |
|------|------|------|---------|
| 纯 GA | `enableRL = false`（默认） | 仅遗传算法：选择 → 交叉 → 变异 → 适应度评估 | 低（几秒/代） |
| GA + RL | `enableRL = true` | GA 每代结束后，对 top-k 个体执行 Q-Learning 精调 | 中高（增加 2-3 倍） |

## 如何开启

### GUI 方式
在"⑤ 遗传算法迷宫优化"面板中：
1. 勾选 **"启用 Q-Learning 精调"** 复选框
2. 设置 **RL 回合**（默认 50，建议 50-200）
3. 设置 **RL Top-K**（默认 3，每代精调几个个体）
4. 点击"运行优化"

### 代码方式
```cpp
OptimizerConfig cfg;
cfg.enableRL = true;          // 开启 RL 精调
cfg.rlEpisodes = 100;         // Q-Learning 训练回合数
cfg.rlTopK = 3;               // 每代精调 top-3 个体
cfg.rlRefineSteps = 5;        // 每个个体每回合的边交换步数
cfg.generations = 30;         // GA 代数
cfg.populationSize = 16;      // 种群大小

MazeOptimizer optimizer;
optimizer.setConfig(cfg);
MazeModel bestMaze = optimizer.run();
```

## Q-Learning 工作原理

### 状态编码（75 个离散状态）
```
特征1: deadEnds / cellCount  → 0-4（5 档，死路密度）
特征2: solutionLength / (rows+cols) → 0-4（5 档，路径长度）
特征3: trapOnMainPath / totalTraps → 0-2（3 档，陷阱暴露率）
状态ID = f1 * 15 + f2 * 3 + f3 → 最多 75 个状态
```

### 动作空间
- 从迷宫的候选边交换操作中选前 10 个
- 每个动作 = 执行一次边交换（删一条边 + 加一条边，保持完美迷宫性质）

### 奖励信号
```
reward = Regret(新迷宫) - Regret(旧迷宫)
Regret = DP最优得分 - Greedy玩家得分
```
- 正奖励：操作让迷宫对 AI 更难（Regret 增大）
- 负奖励：操作让迷宫变简单（Regret 减小）

### Q 表更新
```
Q[s][a] += α * (r + γ * max Q[s'] - Q[s][a])
α = 0.1（学习率）
γ = 0.95（折扣因子）
ε = 0.2 → 0.01（探索率衰减）
```

## 文件清单

| 文件 | 作用 |
|------|------|
| `src/include/qlearning_optimizer.h` | Q-Learning 精调器头文件 |
| `src/source/qlearning_optimizer.cpp` | Q-Learning 精调器实现 |
| `src/include/maze_optimizer.h` | GA 优化器（含 RL 配置字段） |
| `src/source/maze_optimizer.cpp` | GA 循环（含 RL 调用逻辑） |
| `src/source/mainwindow.cpp` | GUI 面板（RL 复选框和参数控件） |

## 服务器运行建议

1. **纯 GA 模式**（快速验证）：`enableRL = false`，几秒完成
2. **GA + RL 模式**（深度优化）：`enableRL = true`，建议参数：
   - `generations = 50`
   - `populationSize = 20`
   - `rlEpisodes = 100`
   - `rlTopK = 5`
3. **纯 GA 先跑，再开 RL**：先用纯 GA 找到不错的种群，再开 RL 精调 top 个体

## 注意事项

- RL 开关默认关闭，不会自动触发训练
- `make` 和 `make test` 只编译和跑自测，**不触发任何优化或训练**
- 需要在 GUI 中点击"运行优化"或在代码中调用 `optimizer.run()` 才会执行
- RL 训练会增加每代耗时约 2-3 倍（取决于 rlEpisodes 和 rlTopK）
