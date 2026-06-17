# 项目运转流程

> 本文档描述整个迷宫设计系统的完整运转流程，包括：
> - 架构总览、线程模型、数据流
> - 每个功能模块的内部流程
> - 用户操作路径（GUI / CLI）
> - 交叉测试对接流程

---

## 一、架构总览

```
┌─────────────────────────────────────────────────────────┐
│                    MainWindow (GUI)                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐  │
│  │ MazeWidget│  │Boss Panel│  │ Opt Panel│  │ Export │  │
│  │ (画布)    │  │ (求解器) │  │ (GA 配置)│  │ (JSON) │  │
│  └──────────┘  └──────────┘  └──────────┘  └────────┘  │
│                                                         │
│  QThread ①: MazeOptimizer (GA worker)                   │
│  QThread ②: GreedyPlayer  (AI worker)                   │
└─────────────────────────────────────────────────────────┘
                           │
          ┌────────────────┼────────────────┐
          ▼                ▼                ▼
    ┌──────────┐    ┌──────────┐    ┌─────────────┐
    │ MazeModel│    │BossSolver│    │MazeOptimizer │
    │ - generate│    │ - solve  │    │ - crossover  │
    │ - DP     │    │ - bounds │    │ - mutate     │
    │ - analyze│    │ - memo   │    │ - evaluate   │
    └──────────┘    └──────────┘    └─────────────┘
          │                                 │
          ▼                                 ▼
    ┌──────────┐                   ┌──────────────┐
    │Resource  │                   │MazeEvaluator │
    │Placer    │                   │ - fitness    │
    └──────────┘                   │ - topoDiff   │
                                   └──────────────┘
```

---

## 二、线程模型

```
Main Thread (GUI)              Worker Thread ① (GA)        Worker Thread ② (AI)
────────────────                ────────────────────        ────────────────────
MazeWidget::paintEvent()       MazeOptimizer::run()         GreedyPlayer::play()
 signal/slot dispatch           for gen 0..N:               while !end:
 QTimer callbacks                selection + crossover        pick best neighbor
                                  + mutation + eval          move + collect
                                  check stopped_             
MainWindow::stopOptimizer()      emit finished(best)        QMetaObject::invokeMethod
  → optimizer_->stop()           → queued to main            → thread->quit()
  → disconnect signals           deleteLater() before quit   
  → quit + wait + cleanup
```

**关键规则**：
- GUI 主线程负责所有 UI 更新、信号处理
- GA 运行在 8MB 栈 QThread，通过 `std::atomic<bool> stopped_` 可中断
- AI 运行在 8MB 栈 QThread，操作 maze 的 deep copy
- `generateMaze()` 开头调用 `stopOptimizer()` + `stopAiWorker()` 保证干净状态
- `generationId_` 计数器防止陈旧回调覆写新迷宫

---

## 三、用户操作全流程

### 3.1 启动
```
启动程序
  │
  ├─ GUI 模式（正常启动）
  │    buildUi() 创建所有控件
  │    algorithmBox_ 默认分治
  │    等待用户操作
  │
  ├─ --self-test             → runSelfTests() → 28 个测试 → 输出 PASS/FAIL
  ├─ --gui-smoke-test        → 打开窗口 200ms 后关闭 → exit 0
  └─ --run-optimizer         → 无头优化器 → 输出 JSON → exit
```

### 3.2 生成迷宫
```
用户：选算法 → 设尺寸/种子 → 点「生成并可视化迷宫」
  │
  ├─ stopOptimizer()         ← 停止旧 GA 线程（如有）
  ├─ stopAiWorker()          ← 停止旧 AI 线程（如有）
  ├─ maze_.generate(rows, cols, algo, seed)
  │    ├─ chooseDiameterEndpoints()  ← 三段 BFS 定 S/E/B
  │    ├─ 调用对应算法生成完美迷宫
  │    └─ carve() 记录每步边到 generationSteps_
  ├─ maze_.placeResources(coins, traps, seed+1)
  ├─ mazeWidget_->setMaze(maze_)
  ├─ generationTimer_->start()   ← 逐边动画
  ├─ updateValidation()          ← 绿色/红色验证标签
  ├─ ++generationId_             ← 陈旧回调守卫
  │
  └─ （如果勾选 GA）→ runOptimizer()
```

### 3.3 DP 求最优路径
```
用户：点「DP 求最优路径」
  │
  ├─ solveResources()
  ├─ maze_.optimalResourceWalk()
  │    ├─ BFS 求 S→E 最短路径主干
  │    ├─ DFS 分支评估：coin+50 / trap-30
  │    │    └─ max(0, calculateGain(child)) 剪掉负收益分支
  │    ├─ appendExcursion() 走入正收益分支
  │    └─ 排除起点/终点，汇总 maxValue
  ├─ 结果显示最大资源值
  └─ pathTimer_ → 蓝色路径逐格动画
```

### 3.4 BOSS 求解
```
用户：填血量→填技能→点「求解最优序列」
  │
  ├─ solveBossBattle()
  ├─ BossSolver::solveWithMaze()
  │    ├─ solve(bossHealth, skills)   ← 分支限界
  │    │    ├─ 贪心初始上界 (bestTurns)
  │    │    ├─ DFS 递归搜索
  │    │    ├─ 乐观下界剪枝 (optimisticTurnLowerBound)
  │    │    └─ 状态记忆剪枝 (哈希)
  │    ├─ maze.optimalResourceWalk()  ← DP 求最大金币
  │    ├─ roundLimit = minTurns + 2
  │    └─ coinConsumption = max(1, maxCoins - 1)  ← 边界压榨
  ├─ 输出：最少回合/最优序列/限定回合/复活金币
  └─ 「战斗动画」→ BattleWindow 独立窗口
```

### 3.5 运行 AI 玩家
```
用户：点「运行贪心 AI」
  │
  ├─ runAiPlayer()
  ├─ 深拷贝 mazeCopy = std::make_shared<MazeModel>(maze_)
  ├─ QThread 启动
  ├─ GreedyPlayer::play(mazeCopy)
  │    ├─ 3×3 局部视野 expanded grid
  │    ├─ 4 种策略：ValuePerStep/NearestFirst/AvoidTraps/EndGoalFirst
  │    ├─ visitCount 防循环
  │    └─ maxSteps = cellCount × 10
  ├─ 线程结束 → finished 回调
  │    ├─ generationId_ 陈旧检查
  │    ├─ 绿色路径动画 (aiPathTimer_)
  │    └─ 显示：剩余资源/步数/效率比值/金币/陷阱
  └─ deleteLater() → 线程清理
```

### 3.6 GA 优化
```
用户：勾选「启用遗传算法优化」→ 设参数 → 点「生成」
  │
  ├─ generateMaze() 首先生成基础迷宫
  ├─ preOptMaze_ = maze_  ← 优化前快照
  ├─ runOptimizer()
  │    ├─ 创建 MazeOptimizer + QThread (8MB 栈)
  │    ├─ generateButton_->setEnabled(false)
  │    ├─ 初始种群：randomChromosome × popSize
  │    │    └─ 生成迷宫拓扑 → 放置资源(对抗/智能/随机)
  │    │
  │    ├─ [循环] for gen 0..generations && !stopped_:
  │    │    ├─ 精英保留 (best → nextGen[0])
  │    │    ├─ 锦标赛选择 (size=3)
  │    │    ├─ 交叉 (union-find 合并边)
  │    │    ├─ 变异 (30% 重生成 / 70% 边交换)
  │    │    ├─ evaluateFitness()
  │    │    │    └─ MazeEvaluator::evaluate()
  │    │    │         ├─ DP 最大资源
  │    │    │         ├─ 4 种贪心策略，取最差分
  │    │    │         ├─ coinMissRate/trapHitRate/pathInefficiency
  │    │    │         ├─ computeTopoDifficulty()
  │    │    │         └─ finalFitness = sum(加权)
  │    │    └─ emit generationFinished(stats)
  │    │
  │    ├─ emit finished(best.maze)
  │    └─ deleteLater() → quit() → 线程清理
  │
  └─ MainWindow::finished 回调
       ├─ generationId_ 陈旧检查
       ├─ maze_ = optimizedMaze_（自动应用）
       ├─ generationTimer_->stop()
       ├─ 对比表：优化前 vs 优化后（4 指标 + 结论）
       ├─ optSaveButton_->setEnabled(true)
       └─ generateButton_->setEnabled(true)
```

### 3.7 导出
```
用户：填 BOSS 信息 → 点「导出 AI 玩家 JSON」
  │
  ├─ exportMaze()
  ├─ BossSolver::solveWithMaze()  ← 自动求解
  ├─ serializeAiPlayerInput()
  │    ├─ maze.expandedGrid()     ← n×n 矩阵
  │    ├─ bossHealth 数组 → "B"
  │    ├─ skills 数组 → "PlayerSkills"
  │    ├─ roundLimit → "minRouds"
  │    └─ coinConsumption → "CoinConsumption"
  └─ QFileDialog → QSaveFile 写入
```

---

## 四、各模块内部流程

### 4.1 MazeModel 生成流程
```
generate(rows, cols, algo, seed)
  ├─ reset() → 初始化网格
  ├─ 调用算法：
  │    ├─ DivideAndConquer: 递归分割，每边界一通道
  │    ├─ KruskalMst:       并查集 + 随机边序
  │    ├─ DFS:              栈迭代 + 随机邻居
  │    └─ BFS:              优先队列 + 12 次尝试取最优
  ├─ chooseDiameterEndpoints()
  │    ├─ BFS①: 从 0 找最远 → startCell_
  │    ├─ BFS②: 从 start 找最远 → endCell_
  │    └─ BFS③: start→end 最短路径 → bossCell_ = parent[end]
  └─ validatePerfect() 自检
```

### 4.2 ResourcePlacer 流程
```
placeResources(coins, traps, seed)
  ├─ BFS 计算 mainPathCells / branchCells
  ├─ 排除 S/E/B
  ├─ 陷阱：至少 1/3 在主路径上，其余随机
  └─ 金币：优先分支，remaining shuffle 后分配

placeSmart (GA 智能模式)
  ├─ 金币：50% 死胡同 → 深分支 → 随机
  └─ 陷阱：非主路分叉口 → 主路径 → 随机

placeAdversarial (GA 对抗模式)
  ├─ 陷阱先放：全部节点 70%
  └─ 金币后放：深分支(branchDepth≥3) → 中深 → 随机（排除死胡同）
```

### 4.3 BossSolver 分支限界流程
```
solve(bossHealth, skills)
  ├─ 前置：必须有 cooldown==0 技能
  ├─ 贪心求上界 bestTurns（每次选最高伤害可用技能）
  ├─ DFS(currentHealth, cooldowns, depth)
  │    ├─ 乐观下界 = optimisticTurnLowerBound()
  │    │    └─ 各技能独立最大次数求和，cap 在 turns×maxDmg
  │    ├─ 剪枝①: depth + optimisticTurns >= bestTurns → 回溯
  │    ├─ 剪枝②: 状态哈希 (health+cooldowns) 已访问 → 回溯
  │    ├─ 更新: currentHealth<=0 → 更新 bestTurns
  │    └─ 递归: 按伤害降序尝试技能
  └─ 返回 bestTurns + skillSequence
```

### 4.4 MazeOptimizer GA 流程
```
run()
  ├─ 初始种群: randomChromosome × popSize
  ├─ evaluateFitness(每个) → 找 best
  │
  └─ for gen 0..generations && !stopped_:
       ├─ nextGen[0] = best (精英保留)
       ├─ while nextGen.size() < popSize:
       │    ├─ tournamentSelect(population) × 2
       │    ├─ crossover(p1, p2) → 子代
       │    │    └─ union-find 合并两父边集 → 补缺边
       │    ├─ mutate(child) 30%概率
       │    │    ├─ 30% 重生成 / 70% 边交换(加边→找环→删环边)
       │    └─ evaluateFitness(child)
       ├─ population = nextGen
       └─ 更新 best
```

### 4.5 MazeEvaluator 适应度计算
```
evaluate(maze, config)
  ├─ 资源放置 (skipPlacement 时跳过)
  ├─ DP: optimalResourceWalk() → dpScore
  ├─ 4 种贪心策略:
  │    ├─ ValuePerStep: 价值/距离（距离硬编码2，即比值排序）
  │    ├─ NearestFirst: 最近优先（实际等价于首次遇到）
  │    ├─ AvoidTraps:    避开陷阱优先
  │    └─ EndGoalFirst:  优先接近终点
  ├─ 取最差分：coinMissRate/trapHitRate/pathInefficiency
  ├─ computeTopoDifficulty()
  │    ├─ 死胡同密度 ×2.0
  │    ├─ 最长走廊/10 ×0.5
  │    ├─ 路径迂回度 ×0.5
  │    └─ 分叉密度 ×1.0 → clamp [0,5]
  └─ finalFitness = coinMissRate×50 + trapHitRate×30
                   + pathInefficiency×20 + topoWeight×topoDifficulty×50
```

---

## 五、数据流

### 5.1 核心数据结构传递
```
MazeModel maze_  ←── 主状态，贯穿全流程
  ├─ generate()     → 拓扑 (passages_)
  ├─ placeResources() → 资源 (resources_)
  ├─ chooseDiameterEndpoints() → S/E/B 位置
  ├─ optimalResourceWalk() → ResourcePlan
  └─ statistics() → MazeStatistics → topoDifficulty

mazeWidget_  ←── 渲染接收者
  ├─ setMaze(maze_)        → 重绘迷宫
  ├─ setRevealCount(n)     → 逐边动画
  ├─ setSolutionPath(walk) → DP 路径
  └─ setAiPath(walk)       → AI 路径

preOptMaze_  → 优化前快照 → 对比表
optimizedMaze_ → GA 最优结果 → 保存/导出
```

### 5.2 JSON 导出流程
```
内部分析           →     MazeModel 格式      →     交叉测试格式
─────────────────      ──────────────           ──────────────────
toJson()               {format, rows, cols,     serializeAiPlayerInput()
                          startCell, endCell,     {maze: [expandedGrid],
                          passages, resources}     B: [HP数组],
                                                   PlayerSkills: [[dmg,cd]],
MazeSaver::saveGAResult()                          minRouds: int,
  {format: "ga-result-v1",                        CoinConsumption: int}
   config: {...},
   bestMaze: {...},
   fitness: ...}
```

---

## 六、交叉测试对接

```
[迷宫组]                              [AI 玩家组]
────────                              ──────────
1. 配置 BOSS + 技能
2. 「导出 AI 玩家 JSON」
3. 得到 5 字段 JSON 文件 ──────────→  4. 导入作为输入
                                     5. AI 探索迷宫
                                     6. 返回 score = resource/steps
                                     
7. 交叉测试矩阵：每个迷宫 × 每个 AI
8. 每个迷宫的得分 = 所有 AI 对其打分的均值
```

---

## 七、CLI 模式

```bash
# 自测 (28 项)
./build/maze_designer --self-test

# GUI 烟雾测试
./build/maze_designer --gui-smoke-test

# 无头优化器
./build/maze_designer --run-optimizer --rows 7 --cols 7

# 无头优化器（自定义参数）
./build/maze_designer --run-optimizer \
  --rows 15 --cols 15 \
  --population 32 \
  --generations 50 \
  --mutation-rate 0.20 \
  --seed 42 \
  --topo-weight 0.3
```

---

## 八、构建系统

```
源码 (src/) 
  │
  ├─ CMakeLists.txt ──→ cmake 配置
  │                       ├─ macOS:  Unix Makefiles / Ninja
  │                       ├─ Linux:  Unix Makefiles / Ninja
  │                       └─ Windows: Visual Studio / Ninja
  │
  ├─ Makefile (Unix)
  │    make / make test / make run / make clean
  │
  └─ build.bat (Windows)
       build.bat / build.bat test / build.bat run / build.bat clean
```
