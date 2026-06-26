# polish 分支优化改动总结

> 分支：`polish`（从 `main` @ `fba0ce2` 分出）  
> 日期：2026-06-26

---

## 1. Goal（目标）

设计并导出迷宫 JSON，交给 **8 组 3×3 视野贪心 AI 玩家** 运行。目标是：

> **最大化不同 AI 玩家得分的区分度（Discrimination），同时难度适中。**
> 不是让 AI 得分低，而是让不同策略的 AI 得分 **拉开差距**——强者高分、弱者低分，具有排名区分力。

### 约束

- 8 组真实 AI 的约束：只能看见周围 3×3（4 个相邻方向），不准用 BFS/DFS/DP/全局搜索
- 迷宫是完美迷宫（树状、V 格 V-1 边、全连通）
- 最终产物是一个 `.json` 迷宫文件

---

## 2. 架构总览

```
迷宫拓扑 ──→ GA 进化边集（Kruskal混合交叉 / 边交换变异 / 锦标赛选择）
    │
    ▼
对抗式资源放置 ──→ 陷阱放非主路径分叉口，金币放分支深处
    │
    ▼
4 种纯 3×3 贪心 AI 评估 ──→ ValuePerStep / NearestFirst / AvoidTraps / EndGoalFirst
    │
    ▼
新 D/B/C 加权公式 ──→ finalFitness = 100 × D^0.55 × B^0.30 × C^0.15
    │
    ▼
GA 选择 ──→ 保留最优，产生下一代
    │
    ▼
导出最优迷宫 JSON
```

---

## 3. 改动清单（共 6 项）

### 3.1 重写适应度公式（核心）

**文件**：`src/source/optimization/maze_evaluator.cpp`

**旧公式**（等权几何平均，60-100 区间）：
```
designGroupScore = 60 + cbrt(D × C × B) × 40
D = clamp01(stddev(resourceRatios) / 0.35)
C = clamp01(reachedCount / 4)
B = clamp01(1 - |meanResourceRatio - 0.45| / 0.18)
```

**新公式**（加权指数，0-100，区分度优先）：
```
// 1. 归一化 AI 得分 [0,1]
aiScore[i] = max(0, remainingResource[i] / totalSteps[i])
idealScore = dpScore / shortestPathLen
normalizedScore[i] = clamp01(aiScore[i] / idealScore)
未到终点 → 0

// 2. 双维度 D：同时考虑标准差和极差
D = sqrt(clamp01(stddev(normalizedScores) / 0.22)
       * clamp01((max-min) / 0.55))

// 3. C 可完成性
C = reachedCount / 4

// 4. B 难度适中（容忍度从 0.18 放宽到 0.30）
B = clamp01(1 - |mean(normalizedScores) - 0.45| / 0.30)

// 5. 加权适应度：D 55%，B 30%，C 15%
finalFitness = 100 × D^0.55 × B^0.30 × C^0.15
```

**设计理念**：
- D 权重 55%：核心目标，直接表达"不同 AI 得分差异"
- D 双维度：`stddev` 捕获整体离散度，`range` 捕获极端差距；几何平均确保两者同时有意义
- B 权重 30%：约束条件，防止迷宫太难（全员失败）或太易（全员满分）
- C 权重 15%：底线约束，迷宫必须可完成
- 归一化用"每步效率"（resource/steps）而非绝对资源量，消除迷宫总资源量的干扰
- 未到终点 AI 得 0 分，防止"全员失败 = 好迷宫"的误判

**新增字段**（`maze_evaluator.h`）：
- `EvalResult::meanAIScoreRatio` — 平均归一化 AI 得分（诊断用）
- `EvalResult::aiScoreSpread` — 归一化得分极差（诊断用）

---

### 3.2 GreedyPlayer 去 BFS 化

**文件**：`src/source/ai/greedy_player.cpp`

**问题**：旧的 GreedyPlayer 虽然用 3×3 视野**选目标**，但**导航**用了 `bfsPath()` 全图最短路径。这违反真实 AI 的"不准 BFS"约束，且高估了 AI 的迷宫导航能力。

**改动**：
- 删除 `bfsPath()` 函数（~20 行）
- 3 种资源策略（ValuePerStep / NearestFirst / AvoidTraps）：目标就是相邻格，**直接踏入**，无需寻路
- EndGoalFirst：在 4 个相邻方向中选**曼哈顿距离离终点最近**的走，每一步重新判断，不预计算全路径
- 卡在死胡同时所有策略回退到"去访问次数最少的邻居"（局部回溯）

**效果**：测试 AI 的导航能力从"完美 BFS"降为"局部贪心"，与真实 8 组 AI 对齐。AI 在长分支、多死胡同的迷宫上会绕更多路，不同策略的路径差异更大 → D 更高。

---

### 3.3 对抗模式微调

**文件**：`src/source/optimization/resource_placer.cpp`

**改动**：`placeAdversarial()` 中，陷阱从放到**所有分叉口**改为只放到**非主路径分叉口**。

```cpp
// 旧：所有分叉口
else if (topo[cell].isJunction) junctions.append(cell);

// 新：只非主路径分叉口
else if (topo[cell].isJunction && !topo[cell].onMainPath)
    junctions.append(cell);
```

**设计理念**：主路径分叉口是 EndGoalFirst 必经之路。如果陷阱全放在必经之路上，EndGoalFirst 被过度惩罚 → C（可完成性）下降。改为放在非主路径分叉口：探索型 AI 踩、直奔终点型 AI 不踩 → 增加区分度，同时保护 C。

---

### 3.4 砍掉 RL 和协同进化

**删除文件**：
- `src/include/ai/rl_player.h`
- `src/source/ai/rl_player.cpp`
- `src/include/optimization/coevolution.h`
- `src/source/optimization/coevolution.cpp`

**修改文件**：
- `src/include/optimization/maze_evaluator.h` — 移除 `evaluateAgainstRL`、`rlConfig`、`rlScore`、`regretRL`
- `src/source/optimization/maze_evaluator.cpp` — 移除 RL 评估死代码块
- `src/source/ui/mainwindow.h` — 移除残留 RL include
- `src/include/io/maze_saver.h/.cpp` — 移除 `saveQTable`/`loadQTable`/`saveCoEvolResult`
- `src/source/main.cpp` — 移除 RL 自测和协同进化自测（共 ~70 行）
- `CMakeLists.txt` — 移除已删文件引用

**原因**：

| 移除项 | 原因 |
|--------|------|
| `evaluateAgainstRL` 配置和代码 | 死代码，始终为 `false`，无调用方 |
| RL 玩家 (`rl_player`) | 只有协同进化使用它，协同进化砍掉后无使用者 |
| 协同进化 (`coevolution`) | 适应度 = `dpScore - RL_score`（单对手遗憾值），与我们"多策略区分度"目标**方向相反**；UI 未集成，仅命令行自测 |
| RL 自测 | flaky 测试（随机种子偶发失败），与区分度目标无关 |
| maze_saver RL 序列化 | 仅协同进化使用 |

**保留**：`qlearning_optimizer`（Q-Learning 边操作精调器）——它不依赖 RL 玩家，是独立的自博弈优化工具。

---

### 3.5 UI 文案更新

**文件**：`src/source/ui/mainwindow.cpp`

- GA 目标标签：`"迷宫组代理分（区分度D × 稳定性C × 均衡性B）"` → `"贪心AI得分区分度 + 难度适中（D⁰·⁵⁵ × B⁰·³⁰ × C⁰·¹⁵）"`
- 进度标签：`"迷宫组代理分"` → `"适应度"`
- 对比表：新增 "AI得分均值比" 和 "AI得分极差" 两行
- 判定文案：`"迷宫组代理分提升"` → `"适应度提升"`

---

### 3.6 自测和文档更新

- `src/source/main.cpp`：增强适应度测试增加 D/C/B/finalFitness 边界校验；GA 对比测试增加新公式输出
- `docs/04-评估指标与优化效果衡量.md`：D/B/C 公式和参数速查表同步更新

---

## 4. 效果验证

### 自测结果（24/24 PASS）

```
PASS enhanced fitness: D=0.504, C=0.75, B=0.167, fitness=38.4, spread=0.267
PASS GA comparison adversarial: D=1, C=0.75, B=0.992, fitness=95.5, spread=0.636
PASS end-to-end: D=1.00, C=0.75, B=0.98, fitness=95.1, spread=0.67
```

### 区分度提升

| | 改动前 | 改动后 |
|---|:--:|:--:|
| 随机迷宫 D | 0 | 0.504 |
| GA 优化后 D | 0.877 | **1.0** |
| GA 优化 fitness | 81.8 | **95.5** |
| AI 得分极差 | 0.467 | **0.636** |

改动前未优化迷宫 D=0（4 种策略因 BFS 导航表现一致），改动后由于去 BFS + 新公式，即使未优化迷宫也有区分度。GA 优化后 D 可达满分。

---

## 5. 不改动的部分

| 组件 | 保持不变 |
|------|---------|
| GA 引擎 | Kruskal 混合交叉、边交换变异、锦标赛选择、精英保留 |
| 迷宫生成 | 四种基础算法（分治/Kruskal/DFS/BFS）、DP 最优路径、拓扑分析 |
| 迷宫验证 | 完美迷宫校验 |
| JSON 导出 | `serializeAiPlayerInput()` 格式不变 |
| Boss 战 | 分支限界求解器不变 |
| 传统 regret 模式 | `useEnhancedFitness=false` 保留 |
| Q-Learning 精调器 | `qlearning_optimizer` 保留（独立模块） |

---

## 6. 测试命令

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
./maze_designer --self-test    # 24 个测试，全部 PASS
```
