# 服务器部署指南 — 迷宫优化器无头运行

> 生成时间：2026-06-15

---

## 一、Git 仓库信息

```
仓库地址：https://github.com/Sigma51815/algorithm-maze-designer.git
分支：tmchao7-maze-dev（最新，已合并 tmchao7-maze-opt 所有内容）
最新 commit：a9d2b7f feat: add headless optimizer mode (--run-optimizer) for server use
```

### 克隆并切换分支

```bash
git clone https://github.com/Sigma51815/algorithm-maze-designer.git
cd algorithm-maze-designer
git checkout tmchao7-maze-dev
```

---

## 二、依赖安装

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev
```

### CentOS/RHEL/Fedora

```bash
sudo dnf install -y gcc-c++ cmake qt6-qtbase-devel
# 或 yum install ...
```

### 验证

```bash
cmake --version   # 需要 >= 3.16
qmake6 --version  # 需要 Qt 6.x
```

---

## 三、构建

```bash
cd algorithm-maze-designer
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles"
make -j$(nproc)
```

构建产物：`build/maze_designer`

---

## 四、运行自测（验证构建正确）

```bash
./build/maze_designer --self-test
```

期望输出：27 个测试全部 PASS，最后一行 `ALL TESTS PASSED`。

---

## 五、无头优化器运行

### 基本命令

```bash
./build/maze_designer --run-optimizer [参数...]
```

### 所有可用参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--rows N` | 7 | 迷宫格子行数（实际矩阵 2N+1） |
| `--cols N` | 7 | 迷宫格子列数 |
| `--population N` | 16 | GA 种群大小 |
| `--generations N` | 30 | GA 进化代数 |
| `--mutation-rate X` | 0.15 | 变异率（0.0-1.0） |
| `--coins N` | 8 | 金币数量 |
| `--traps N` | 5 | 陷阱数量 |
| `--seed N` | 42 | 随机种子（可复现） |
| `--enable-rl N` | 0 | 是否开启 RL 精调（0=关 1=开） |
| `--rl-episodes N` | 50 | RL 训练回合数 |
| `--topo-weight X` | 0.3 | 拓扑难度权重 |

### 推荐运行方案

**方案 A：快速测试（<1 分钟）**

```bash
./build/maze_designer --run-optimizer \
  --rows 5 --cols 5 \
  --population 8 --generations 10 \
  --coins 5 --traps 3 --seed 42
```

**方案 B：标准优化（~2 分钟）**

```bash
./build/maze_designer --run-optimizer \
  --rows 7 --cols 7 \
  --population 16 --generations 30 \
  --coins 10 --traps 6 --seed 202506
```

**方案 C：带 RL 训练（~5-10 分钟）**

```bash
./build/maze_designer --run-optimizer \
  --rows 7 --cols 7 \
  --population 16 --generations 30 \
  --coins 10 --traps 6 \
  --enable-rl 1 --rl-episodes 10000 \
  --seed 202506
```

**方案 D：大规模优化（~30 分钟，服务器推荐）**

```bash
./build/maze_designer --run-optimizer \
  --rows 7 --cols 7 \
  --population 30 --generations 100 \
  --coins 10 --traps 6 \
  --enable-rl 1 --rl-episodes 20000 \
  --topo-weight 0.5 \
  --seed 202506
```

---

## 六、输出格式

### 实时输出（stdout）

```
=== Headless Optimizer ===
Maze: 7x7 | Pop: 16 | Gen: 30
Coins: 10 | Traps: 6 | Mutation: 15%
Smart placement: ON | Enhanced fitness: ON | RL: OFF

Gen   1/30 | best=286.0 avg=192.3 | dp=190 greedy=-30 regret=220
Gen   2/30 | best=310.0 avg=210.5 | dp=210 greedy=-10 regret=220
...

=== Result ===
DP optimal: 310
Greedy worst: -60
Regret: 370
Topo difficulty: 1.16
Final fitness: 515.7
Valid: YES
Saved: optimizer_result.json
```

### 输出文件

- `optimizer_result.json` — 最优迷宫的 AI 玩家交叉测试格式 JSON
- 包含迷宫矩阵、BOSS 参数、技能配置

---

## 七、项目技术栈

- 语言：C++17
- GUI 框架：Qt 6 Widgets（仅 GUI 模式需要）
- 构建：CMake + Unix Makefiles
- 无头模式：仅需 Qt6 Core（不需要 Widgets/GUI）

---

## 八、关键源文件

```
src/
  include/
    maze.h              # 迷宫核心（4 种算法、DP、验证）
    maze_optimizer.h    # GA 优化器
    maze_evaluator.h    # 增强适应度评估
    resource_placer.h   # 智能资源分布
    ai/
      greedy_player.h   # 3×3 贪心 AI
      rl_player.h       # Q-Learning RL 玩家
  source/
    main.cpp            # 入口 + --self-test + --run-optimizer
    maze_optimizer.cpp  # GA 实现
    maze_evaluator.cpp  # 适应度实现
    ai/rl_player.cpp    # RL 训练实现
```

---

## 九、注意事项

1. **CPU 负载**：RL 训练单核 100%，GA 多代运行时持续满载。服务器无风扇也没问题（纯计算，无 GPU）
2. **内存**：Q 表 32MB + 种群数据，总计 <500MB
3. **种子可复现**：相同 seed + 参数 = 相同结果
4. **无头模式不需要 X11/显示**：用 `QCoreApplication`，不需要 `QApplication`
