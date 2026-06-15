# 任务计划：遗传算法迷宫优化器

## 目标
实现 PAIRED 思想的遗传算法迷宫优化器——通过 GA 进化迷宫种群，最大化 Regret = DP最优 - Greedy玩家得分，生成对 AI 玩家最具挑战性的迷宫。

## 当前阶段
阶段 1

## 各阶段

### 阶段 1：核心优化器实现
- [x] MazeOptimizer 头文件（配置、信号、接口）
- [x] MazeOptimizer 实现（染色体编码、交叉、变异、适应度评估、GA主循环）
- **状态：** complete

### 阶段 2：GUI 集成
- [x] MainWindow 添加优化器面板（种群大小、代数、变异率、算法选择）
- [x] 进度可视化（每代适应度、DP/Greedy 对比）
- [x] 应用优化结果到当前迷宫
- **状态：** complete

### 阶段 3：测试与验证
- [x] 编译通过
- [x] 19 项自回归测试全部通过
- [ ] GUI 功能测试（需手动运行）
- **状态：** in_progress

## 关键技术决策

### 染色体编码
完美迷宫 = 生成树（N×N 个节点的网格图的生成树）
- 染色体 = 有序的边列表（从全部候选边中选 N²-1 条）
- 交叉：两个父代的边集取并集，用 Union-Find 从并集中随机选边构建子代生成树
- 变异：随机替换一条边（删一条加一条），保持生成树性质

### 适应度函数（PAIRED Regret）
```
Fitness(maze) = DP_optimal(maze) - Greedy_agent(maze)
```
- DP_optimal = optimalResourceWalk().maxValue
- Greedy_agent = GreedyPlayer::play().remainingResource
- 资源分布策略：与现有 placeResources 一致

### 协同进化
- Adversary（迷宫生成器）：GA 进化迷宫种群
- Protagonist（Greedy 玩家）：固定策略，作为评估基准
- 可选：周期性升级 Protagonist 策略

## 已做决策
| 决策 | 理由 |
|------|------|
| 使用生成树编码 | 保证完美迷宫约束，变异/交叉操作天然保持连通性 |
| PAIRED Regret 作为适应度 | 方案甲 §五的核心思想，最大化 DP 与 Greedy 的差距 |
| 先实现基础 GA，再考虑 GAN 协同进化 | 渐进式开发，确保基础可用 |

## 遇到的错误
| 错误 | 尝试次数 | 解决方案 |
|------|---------|---------|
|      |         |         |

## 备注
- 分支：tmchao7-maze-optimizer（基于 tmchao7-maze-dev）
- 已有基础设施：MazeModel（4种生成算法）、optimalResourceWalk（DP）、GreedyPlayer
- 核心创新：将迷宫优化建模为进化搜索问题
