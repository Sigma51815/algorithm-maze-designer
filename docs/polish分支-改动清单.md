# polish 分支改动清单（→ main）

> **分支**：`polish`（从 `main` @ `fba0ce2`「boss逻辑优化」分出）
> **合并提交**：`22930b7` — `polish: 优化迷宫区分度 — 新D/B/C公式 + 纯3x3贪心AI + 砍RL协同进化`
> **日期**：2026-06-26
> **自测**：`./maze_designer --self-test` → **ALL TESTS PASSED**（exit 0）

---

## 0. 一句话总结

重写迷宫适应度公式（以「不同 AI 得分区分度」为核心目标），把测试 AI 从「BFS 全图寻路」降级为「纯 3×3 局部贪心」以对齐真实玩家约束，并砍掉与目标方向相反的 RL / 协同进化模块，使 GA 优化后的迷宫在 4 种贪心 AI 上的区分度 D 达到满分。

---

## 1. 改动规模

| 项目 | 数量 |
|------|------|
| 提交数（领先 main） | 1 |
| 变更文件总数 | 23 |
| 新增文件 | 3 |
| 修改文件 | 12 |
| 删除文件 | 8 |
| 代码行 | +810 / −1666 |

> 注：`22930b7` 合并前曾新增 7 份说明文档；后续 `8ba12e1` 清理了其中 5 份扩展文档和 2 份旧流程文档。此表按当前 `main` 最终状态统计。

---

## 2. 文件变更清单

### 2.1 新增（3）
| 文件 | 说明 |
|------|------|
| `docs/polish-优化改动总结.md` | 本次 polish 的设计思路总结 |
| `docs/polish分支-改动清单.md` | 当前分支相对 main 的文件级改动清单 |
| `goal.md` | 项目目标与阅读路线图 |

### 2.2 修改（12）
| 文件 | 说明 |
|------|------|
| `src/source/optimization/maze_evaluator.cpp` | **核心**：重写 D/B/C 适应度公式 |
| `src/include/optimization/maze_evaluator.h` | 新增诊断字段；移除 RL 字段 |
| `src/source/ai/greedy_player.cpp` | **核心**：去 BFS，纯 3×3 局部贪心 |
| `src/include/ai/greedy_player.h` | 策略枚举 `NearestFirst` → `CautiousCollector` |
| `src/source/optimization/maze_optimizer.cpp` | 同步策略枚举重命名 |
| `src/source/optimization/resource_placer.cpp` | 对抗式陷阱只放非主路径分叉口 |
| `src/source/io/maze_saver.cpp` | 移除 RL/协同进化序列化代码 |
| `src/include/io/maze_saver.h` | 移除 `saveQTable`/`loadQTable`/`saveCoEvolResult` |
| `src/source/main.cpp` | 移除 RL/协同进化自测；增强适应度自测 |
| `src/source/ui/mainwindow.cpp` | UI 文案对齐新公式 |
| `src/include/ui/mainwindow.h` | 移除残留 RL include |
| `CMakeLists.txt` | 移除已删文件引用 |

### 2.3 删除（8）
| 文件 | 说明 |
|------|------|
| `src/include/ai/rl_player.h` | RL 玩家（仅协同进化使用，已砍） |
| `src/source/ai/rl_player.cpp` | 同上 |
| `src/include/optimization/coevolution.h` | 协同进化（目标方向相反，已砍） |
| `src/source/optimization/coevolution.cpp` | 同上 |
| `build.bat` | 旧 Windows 构建脚本 |
| `运行迷宫设计.bat` | 旧 Windows 运行脚本 |
| `docs/PROJECT_WORKFLOW.md` | 旧流程文档，内容已被 README 和现有 docs 替代 |
| `docs/PROJ_GUIDE.md` | 旧交接文档，内容已被 README 和现有 docs 替代 |

---

## 3. 核心改动详解

### 3.1 重写适应度公式（最核心）

**文件**：`src/source/optimization/maze_evaluator.cpp`、`maze_evaluator.h`

**旧公式**（等权几何平均，值域 60–100）：
```cpp
D = clamp01(resourceStddev / 0.35)
C = clamp01(reachedCount / 4)
B = clamp01(1 - |meanResourceRatio - 0.45| / 0.18)
designGroupScore = 60 + cbrt(D * C * B) * 40
```

**新公式**（加权指数，值域 0–100，区分度优先）：
```cpp
// 1. 归一化每个 AI 得分到 [0,1]（每步效率，消除总资源量干扰）
idealScore   = dpScore / shortestPathLen      // 理论最优每步得分
aiScore[i]   = max(0, remainingResource[i]) / totalSteps[i]
normalizedScore[i] = clamp01(aiScore[i] / idealScore)
未到终点 → 0                                  // 防止「全员失败=好迷宫」

// 2. D 区分度 = stddev 维度与极差维度的调和平均（双维度）
dStddev = clamp01(stddev(normalizedScores) / 0.22)
dRange  = clamp01((max - min)            / 0.55)
D = 2 * dStddev * dRange / (dStddev + dRange)   // 调和平均，任一为0则D=0

// 3. C 可完成性 = 到达终点的 AI 比例
C = reachedCount / 4

// 4. B 难度适中（容忍度 0.18 → 0.30，更宽松）
B = clamp01(1 - |mean(normalizedScores) - 0.45| / 0.30)

// 5. 加权适应度：D 55% / B 30% / C 15%
finalFitness = 100 * D^0.55 * B^0.30 * C^0.15
```

**设计要点**：
- D 权重 55%，直接表达「不同 AI 得分差异」这一核心目标。
- D 用**调和平均**（`2ab/(a+b)`）合并 stddev 与极差两个维度——任一维度为 0 则 D 为 0，确保「整体离散」与「极端差距」同时成立。（自测 `boundary harmonic mean: H(0,0.8)=0, H(0.6,0.6)=0.6` 验证。）
- 归一化用「每步效率」而非绝对资源量，消除迷宫总资源量对得分的干扰。
- 未到终点 AI 得 0 分，防止「全员失败」被误判为高区分度。
- B 容忍度从 0.18 放宽到 0.30，避免难度约束过严压制区分度。

**新增诊断字段**（`maze_evaluator.h`）：
- `EvalResult::meanAIScoreRatio` — 归一化 AI 得分均值
- `EvalResult::aiScoreSpread` — 归一化得分极差

---

### 3.2 GreedyPlayer 去 BFS 化 + 策略重命名

**文件**：`src/source/ai/greedy_player.cpp`、`greedy_player.h`、`maze_optimizer.cpp`、`maze_evaluator.cpp`

**问题**：旧 GreedyPlayer 虽用 3×3 视野**选目标**，但**导航**用了 `bfsPath()` 全图最短路径。这违反真实 AI「不准 BFS」约束，且高估了 AI 导航能力——导致 4 种策略表现趋同、D≈0。

**改动**：
- **删除 `bfsPath()`**（~20 行）。
- **策略枚举重命名**：`NearestFirst` → `CautiousCollector`。
- **EndGoalFirst**：在 4 个相邻方向中选**曼哈顿距离离终点最近**的走，每步重新判断，不预计算全路径。
- **资源策略**（ValuePerStep / CautiousCollector / AvoidTraps）：从可见相邻格中按 `value / dist` 选最优，**直接踏入**，无寻路。
  - `CautiousCollector` 只拿正价值格；探索（无可见资源）时偏好朝终点方向（最少访问 + 曼哈顿最近）。
- **死胡同路回退**：所有策略回退到「去访问次数最少的邻居」做局部回溯。

**效果**：测试 AI 导航从「完美 BFS」降为「局部贪心」，与真实 8 组 3×3 AI 对齐。AI 在长分支 / 多死胡同迷宫上绕更多路，不同策略路径差异更大 → D 显著上升。

---

### 3.3 对抗式资源放置微调

**文件**：`src/source/optimization/resource_placer.cpp`

**改动**：`placeAdversarial()` 中，陷阱从放到**所有分叉口**改为只放到**非主路径分叉口**：
```cpp
// 旧：所有分叉口
else if (topo[cell].isJunction) junctions.append(cell);
// 新：只非主路径分叉口
else if (topo[cell].isJunction && !topo[cell].onMainPath) junctions.append(cell);
```

**设计理念**：主路径分叉口是 EndGoalFirst 必经之路。若陷阱全放必经之路，EndGoalFirst 被过度惩罚 → C（可完成性）下降。改为放非主路径分叉口：探索型 AI 踩、直奔终点型 AI 不踩 → 增加区分度，同时保护 C。

---

### 3.4 砍掉 RL 和协同进化

**删除文件**：`rl_player.h/.cpp`、`coevolution.h/.cpp`
**连带清理**：`maze_evaluator.h`（移除 `evaluateAgainstRL`/`rlConfig`/`rlScore`/`regretRL`）、`maze_saver.h/.cpp`（移除 `saveQTable`/`loadQTable`/`saveCoEvolResult` 及 `rlScore` 字段）、`mainwindow.h`（移除 RL include）、`main.cpp`（移除 RL/协同进化自测 ~70 行）、`CMakeLists.txt`（移除已删文件引用）。

| 移除项 | 原因 |
|--------|------|
| `evaluateAgainstRL` 配置与代码 | 死代码，始终 `false`，无调用方 |
| RL 玩家 `rl_player` | 仅协同进化使用，协同进化砍掉后无使用者 |
| 协同进化 `coevolution` | 适应度 = `dpScore - RL_score`（单对手遗憾值），与「多策略区分度」目标**方向相反**；且 UI 未集成，仅命令行自测 |
| RL 自测 | flaky（随机种子偶发失败），与区分度目标无关 |
| `maze_saver` RL 序列化 | 仅协同进化使用 |

**保留**：`qlearning_optimizer`（Q-Learning 边操作精调器）——不依赖 RL 玩家，是独立的自博弈优化工具，本次未改动。

---

### 3.5 UI 文案更新

**文件**：`src/source/ui/mainwindow.cpp`
- GA 目标标签：`"迷宫组代理分（区分度D × 稳定性C × 均衡性B）"` → `"贪心AI得分区分度 + 难度适中（D⁰·⁵⁵ × B⁰·³⁰ × C⁰·¹⁵）"`
- 进度标签：`"迷宫组代理分"` → `"适应度"`
- 对比表：新增「AI得分均值比」「AI得分极差」两行
- 判定文案：`"迷宫组代理分提升"` → `"适应度提升"`

---

### 3.6 自测增强 + 文档新增

- `src/source/main.cpp`：增强适应度测试加入 D/C/B/finalFitness 边界校验；GA 对比测试输出新公式结果；**新增**「调和平均边界」「策略区分度」测试用例。
- 当前保留 3 份新增文档（见 2.1）；合并前的 5 份扩展说明文档已在后续清理提交中删除。

---

## 4. 效果验证（`./maze_designer --self-test`，全部 PASS）

关键测试输出（实测）：
```
PASS enhanced fitness: dp=150, regret=180, topo=0.8, D=0.504063, C=0.75, B=0.166667, fitness=38.3867, meanRatio=0.2, spread=0.266667
PASS boundary small maze (3x3): D=1, C=0.75, B=0.75, fitness=87.8572
PASS boundary harmonic mean: H(0,0.8)=0, H(0.6,0.6)=0.6
PASS strategy discrimination: 6/6 pairs differ, scores=[-60,-40,-10,0]
PASS GA comparison:
  random:      dp=250 greedy=-60 regret=310 ai_score=2.5    ai_steps=64
  smart:       dp=200 greedy=0   regret=200 ai_score=4.4186 ai_steps=43
  adversarial: dp=220 greedy=0   regret=220 ai_score=3.137  ai_steps=51  D=1 C=0.75 B=0.999736 fitness=95.769 spread=0.687384
PASS end-to-end: fitness=94.9, dp=190, greedy=0, regret=190, topo=0.81, D=1.00, C=0.75, B=0.97, spread=0.61
ALL TESTS PASSED
```

区分度提升对比：

| | 改动前 | 改动后 |
|---|:--:|:--:|
| 随机迷宫 D | 0 | 0.504 |
| GA 优化后 D | 0.877 | **1.0** |
| GA 优化 fitness | 81.8 | **95.8** |
| AI 得分极差 spread | 0.467 | **0.687** |

改动前未优化迷宫 D=0（4 种策略因 BFS 导航表现一致）；改动后去 BFS + 新公式，即使未优化迷宫也有区分度，GA 优化后 D 可达满分。

---

## 5. 保持不变的部分

| 组件 | 状态 |
|------|------|
| GA 引擎 | Kruskal 混合交叉 / 边交换变异 / 锦标赛选择 / 精英保留，不变 |
| 迷宫生成 | 四种基础算法（分治 / Kruskal / DFS / BFS），不变 |
| DP 最优路径 | 不变 |
| 迷宫验证 | 完美迷宫校验，不变 |
| JSON 导出 | `serializeAiPlayerInput()` 格式不变 |
| Boss 战 | 分支限界求解器不变 |
| 传统 regret 模式 | `useEnhancedFitness=false` 保留 |
| Q-Learning 精调器 | `qlearning_optimizer` 保留（独立模块） |

---

## 6. 复现 / 验证命令

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
./maze_designer --self-test    # 全部 PASS
```

---

## 7. 合并信息

- **合并方式**：fast-forward（`main` 为 `polish` 的直接祖先，无分叉）
- **polish 合并提交**：`22930b7`
- **当前 main HEAD**：`8ba12e1`（在 polish 合并后删除不必要文档）
- 当前 main 的代码变更与已通过自测的 polish 工作树一致，差异主要是文档清理。
