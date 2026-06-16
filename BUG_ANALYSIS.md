# Bug：GA 优化后 AI 分数反而更高

## 现象

- 不用 GA 优化：ai_score = 0.351
- 用 GA 优化后：ai_score = 1.167
- 期望：GA 优化后 ai_score 应该更低

## 已确认的根因

### 1. evaluateFitness 重新放置资源导致不一致

**位置**：`src/source/maze_optimizer.cpp:155-163`

```cpp
double MazeOptimizer::evaluateFitness(Chromosome &chrom) {
    if (config_.useEnhancedFitness) {
        EvaluatorConfig ec;
        ec.useAdversarialPlacement = config_.useAdversarialPlacement;
        ec.useSmartPlacement = !config_.useAdversarialPlacement && config_.useSmartPlacement;
        ec.placerConfig.seed = rng_();  // ← 每次用不同 seed
        EvalResult eval = MazeEvaluator::evaluate(chrom.maze, ec);  // ← 重新放置资源
        ...
    }
}
```

**问题**：
- `randomChromosome()` 或 `mutate()` 已经放置了资源
- `evaluateFitness()` 又用新的 seed 重新放置资源
- 染色体的 `fitness` 是基于**新资源**计算的
- 但染色体的 `maze` 里存的是**最后一次评估的资源**
- 交叉/变异后，子代资源被覆盖为新 seed 的版本
- **导致**：最优迷宫的资源分布与评估时不同，可能正好帮了 AI

### 2. 无头优化器还在用 Smart Placement

**位置**：`src/source/main.cpp:844`

```cpp
cfg.useSmartPlacement = true;  // ← 没有 useAdversarialPlacement
```

无头模式 `--run-optimizer` 默认用智能放置（金币放死胡同），不是对抗模式。

### 3. 适应度与 AI score 的脱节

**当前适应度**（对抗模式）：`fitness = -(greedy worst) + topo bonus`

**AI score**：`remainingResource / totalSteps`

**问题**：
- 适应度只看 `remainingResource`（贪婪最差分）
- 但 AI score 还除以了 `totalSteps`
- 一个迷宫可能 greedy 分很低（-180），但步数也很少（47），导致 ai_score = -180/47 = -3.8
- 另一个迷宫 greedy 分较高（-60），但步数很多（200），ai_score = -60/200 = -0.3
- 后者的 ai_score 更高（-0.3 > -3.8），但适应度更低

**结论**：适应度函数没有直接优化 ai_score（resource/steps），而是优化了 resource 的负值。

## 可能的修复方向

### A. 修复资源不一致问题
在 `evaluateFitness()` 中，不要重新放置资源，使用染色体已有的资源：
```cpp
// 不调用 ResourcePlacer，直接评估已有资源
ec.useSmartPlacement = false;
ec.useAdversarialPlacement = false;
```

### B. 适应度直接使用 ai_score
```cpp
PlayResult r = GreedyPlayer::play(maze);
double aiScore = static_cast<double>(r.remainingResource) / r.totalSteps;
fitness = -aiScore;  // 最小化 ai_score
```

### C. 修复无头优化器默认模式
```cpp
cfg.useAdversarialPlacement = true;
cfg.useSmartPlacement = false;
```

## 关键文件

| 文件 | 问题 |
|------|------|
| `src/source/maze_optimizer.cpp:155-163` | evaluateFitness 重新放置资源 |
| `src/source/main.cpp:844` | 无头优化器用 smart 不用 adversarial |
| `src/source/maze_evaluator.cpp:8-55` | 适应度函数未直接优化 ai_score |
| `src/source/mainwindow.cpp:1019` | GUI 优化器配置 |
