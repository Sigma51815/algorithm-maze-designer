# 方案乙：AI 玩家设计任务

> **目标**：设计智能体在 3×3 局部视野下穿越迷宫、收集资源、击败 BOSS  
> **核心算法**：贪心策略、Q-Learning、DRQN（深度循环 Q 网络）  
> **进阶策略**：课程学习、BOSS 战策略解耦

---

## 一、任务分析

### 1.1 评分机制

AI 玩家的核心考核指标：

$$
\text{Score}_{\text{AI}} = \frac{R_{\text{remaining}}}{S_{\text{total}}}
$$

- $R_{\text{remaining}}$：抵达终点时剩余的资源价值（金币 50，陷阱 -30）
- $S_{\text{total}}$：总步数

**设计目标**：在尽可能少的步数内收集尽可能多的净资源。

### 1.2 核心挑战

| 挑战 | 说明 |
|:---:|:---:|
| 部分可观测 | 3×3 视野，无法感知全局状态 |
| 诱导陷阱 | 迷宫设计者会在视野边缘放置诱导性资源 |
| 步数惩罚 | 每一步都有成本，需避免无效探索 |
| BOSS 战 | 需保留足够金币击败 BOSS |

---

## 二、环境建模（Gymnasium 接口）

### 2.1 状态空间

| 维度 | 内容 | 取值范围 |
|:---:|:---:|:---:|
| 3×3×C | 局部视野 | C 为通道数（墙壁/通路/金币/陷阱/BOSS） |
| 1 | 当前金币数 | 归一化到 [0, 1] |
| 1 | 当前血量 | 归一化到 [0, 1] |
| H | LSTM 隐状态 | 记忆历史轨迹（DRQN 专用） |

### 2.2 动作空间

$$
\mathcal{A} = \{\text{上},\ \text{下},\ \text{左},\ \text{右}\}
$$

### 2.3 奖励函数

$$
R_t = \beta_1 \cdot \Delta \text{Gold} - \beta_2 \cdot \Delta \text{Trap} - \beta_3 \cdot \text{Step} + \beta_4 \cdot \text{Terminal}
$$

| 参数 | 建议值 | 说明 |
|:---:|:---:|:---:|
| $\beta_1$ | 1.0 | 金币奖励系数 |
| $\beta_2$ | 0.6 | 陷阱惩罚系数 |
| $\beta_3$ | 0.01 | 步数惩罚系数 |
| $\beta_4$ | 10.0 | 到达终点奖励 |

### 2.4 环境接口伪代码

```
class MazeEnv:
    def __init__(self, maze_matrix):
        self.maze = maze_matrix
        self.player_pos = start_pos
        self.player_gold = 0
        self.player_hp = 100
        self.steps = 0

    def reset(self):
        self.player_pos = start_pos
        self.player_gold = 0
        self.player_hp = 100
        self.steps = 0
        return self.get_3x3_view()

    def step(self, action):
        new_pos ← 计算新位置（上下左右）

        if 新位置为墙壁:
            return self.get_3x3_view(), -1, False, {}

        self.player_pos = new_pos
        self.steps += 1

        reward ← -0.01  // 步数惩罚
        if 新位置为金币:
            self.player_gold += 50
            reward += 1.0
            maze[new_pos] = 通路  // 金币被拾取
        elif 新位置为陷阱:
            self.player_hp -= 30
            reward -= 0.6
            maze[new_pos] = 通路  // 陷阱触发一次

        done ← 到达终点 or hp ≤ 0
        if 到达终点:
            reward += 10.0

        return self.get_3x3_view(), reward, done, {gold, hp, steps}

    def get_3x3_view(self):
        // 返回以玩家为中心的 3×3 区域
        // 超出边界的部分视为墙壁
        return self.maze[player_pos.y-1 : player_pos.y+2,
                         player_pos.x-1 : player_pos.x+2]
```

---

## 三、基线方案：3×3 贪心策略

### 3.1 算法描述

每次移动时，在 3×3 视野内选择"性价比"最高的资源。

**性价比定义**：

$$
\text{Value}(r) = \frac{\text{resource\_value}(r)}{\text{distance}(player, r)}
$$

### 3.2 伪代码

```
function GreedyPolicy(observation, player_pos):
    candidates ← 在 3×3 视野内识别所有可见资源

    if candidates 为空:
        return 随机选择一个可行方向

    best_resource ← argmax(Value(r)) for r in candidates
    direction ← 计算从 player_pos 到 best_resource 的移动方向

    if direction 方向为墙壁:
        return 随机选择一个可行方向

    return direction
```

### 3.3 局限性分析

- **伪梯度诱导**：迷宫设计者可在视野边缘放置高价值金币诱导 AI 进入死胡同
- **缺乏记忆**：无法记住已探索区域，容易重复探索
- **无长远规划**：只看当前视野，无法预判后续路径

---

## 四、进阶方案 A：Q-Learning（表格型）

### 4.1 适用场景

- 迷宫尺寸较小（$\leq 15 \times 15$）
- 状态空间可离散化

### 4.2 算法设计

**状态编码**：将 3×3 视野编码为一个离散状态

$$
s = \sum_{i=0}^{8} \text{view}[i] \times 5^i
$$

其中 $\text{view}[i] \in \{0, 1, 2, 3, 4\}$（墙壁/通路/金币/陷阱/BOSS）。

**Q 表更新**：

$$
Q(s, a) \leftarrow Q(s, a) + \alpha \left[ r + \gamma \max_{a'} Q(s', a') - Q(s, a) \right]
$$

### 4.3 伪代码

```
function QLearning_Train(env, episodes, alpha, gamma, epsilon):
    Q ← 初始化 Q 表（全零）

    for episode = 1 to episodes:
        s ← env.reset()
        done ← False

        while not done:
            // ε-贪心策略
            if random() < epsilon:
                a ← 随机动作
            else:
                a ← argmax Q(s, a)

            s_next, r, done, info ← env.step(a)

            // Q 表更新
            Q[s, a] ← Q[s, a] + alpha * (r + gamma * max(Q[s_next, :]) - Q[s, a])
            s ← s_next

        epsilon ← max(0.01, epsilon * 0.995)  // 衰减探索率

    return Q
```

### 4.4 复杂度分析

- **时间复杂度**：$O(\text{episodes} \times \text{max\_steps})$
- **空间复杂度**：$O(|\mathcal{S}| \times |\mathcal{A}|)$（Q 表大小）

**注意**：当迷宫较大时，状态空间 $|\mathcal{S}| = 5^9 = 1,953,125$，Q 表可能过大。此时应考虑使用 DRQN。

---

## 五、进阶方案 B：DRQN（深度循环 Q 网络）

### 5.1 算法原理

Hausknecht & Stone (2015) 提出的 DRQN 在 DQN 基础上引入 LSTM 层，使智能体能够记忆历史轨迹，适用于 POMDP 环境。

**核心改进**：将 DQN 的全连接层替换为 LSTM 层，使网络能够处理序列输入。

### 5.2 网络架构

$$
\text{Input}(3 \times 3 \times C) \rightarrow \text{Conv2D}(16, 3 \times 3) \rightarrow \text{Flatten} \rightarrow \text{LSTM}(128) \rightarrow \text{FC}(4) \rightarrow Q(a)
$$

### 5.3 伪代码

```
class DRQN:
    def __init__(self):
        self.conv ← Conv2D(in_channels=C, out_channels=16, kernel_size=3)
        self.lstm ← LSTM(input_size=144, hidden_size=128)
        self.fc ← Linear(128, 4)  // 4 个动作

    def forward(self, observation, hidden_state):
        x ← conv(observation)
        x ← flatten(x)
        x, hidden_state ← lstm(x, hidden_state)
        q_values ← fc(x)
        return q_values, hidden_state
```

### 5.4 训练流程

```
function DRQN_Train(env, episodes):
    model ← DRQN()
    target_model ← DRQN()  // 目标网络
    replay_buffer ← SequenceReplayBuffer()

    for episode = 1 to episodes:
        s ← env.reset()
        h ← None  // LSTM 隐状态
        done ← False

        while not done:
            q, h ← model(s, h)

            // ε-贪心
            a ← ε_greedy(q, epsilon)

            s_next, r, done, info ← env.step(a)
            replay_buffer.add(s, a, r, s_next, done, h)

            s ← s_next

            // 从 replay buffer 采样序列
            if replay_buffer.size() > batch_size:
                batch ← replay_buffer.sample_sequences(batch_size, seq_len=32)
                loss ← compute_loss(batch, model, target_model)
                loss.backward()
                optimizer.step()

        // 定期更新目标网络
        if episode % target_update_freq == 0:
            target_model ← copy(model)

    return model
```

### 5.5 复杂度分析

- **时间复杂度**：$O(\text{episodes} \times \text{max\_steps} \times \text{forward\_pass})$
- **空间复杂度**：$O(\text{replay\_buffer\_size} + \text{model\_parameters})$

---

## 六、进阶方案 C：PPO + LSTM（策略梯度）

### 6.1 适用场景

- 动作空间较大或连续
- 需要更稳定的训练过程

### 6.2 核心思想

PPO（Proximal Policy Optimization）通过裁剪目标函数限制策略更新幅度，保证训练稳定性。

**目标函数**：

$$
L^{CLIP}(\theta) = \mathbb{E}_t \left[ \min \left( r_t(\theta) A_t,\ \text{clip}(r_t(\theta), 1-\epsilon, 1+\epsilon) A_t \right) \right]
$$

其中 $r_t(\theta) = \frac{\pi_\theta(a_t|s_t)}{\pi_{\theta_{old}}(a_t|s_t)}$，$A_t$ 为优势函数。

### 6.3 伪代码

```
function PPO_Train(env, episodes):
    actor_critic ← ActorCritic_LSTM()

    for episode = 1 to episodes:
        trajectories ← collect_trajectories(env, actor_critic)

        for each trajectory in trajectories:
            // 计算优势函数
            advantages ← GAE(trajectory.rewards, trajectory.values)

            // PPO 更新
            for epoch = 1 to K:
                ratio ← π_new(a|s) / π_old(a|s)
                loss ← -min(ratio * advantages,
                            clip(ratio, 1-ε, 1+ε) * advantages)
                loss.backward()
                optimizer.step()

    return actor_critic
```

---

## 七、课程学习与泛化性训练

### 7.1 三阶段训练流水线

```
阶段一：常规随机迷宫（~1000 个）
    目标：学会基础行为（吃金币、避墙、到达终点）
    算法：Q-Learning 或 DRQN

          ↓

阶段二：对抗性迷宫（~500 个）
    目标：学会高级决策（抵御诱导陷阱、权衡绕路与风险）
    数据源：组内生成的极端迷宫

          ↓

阶段三：跨组验证
    目标：泛化性检验，超参微调
    数据源：其他组提供的迷宫样本
```

### 7.2 数据增强策略

- **随机旋转/翻转**：增加训练数据多样性
- **资源分布扰动**：微调金币/陷阱位置
- **迷宫拓扑变异**：使用遗传算法生成变体迷宫

---

## 八、BOSS 战策略解耦

### 8.1 设计思想

BOSS 战是离散回合制且有明确的技能/复活机制，其最优策略可以通过**精确算法**（分支限界/DP）求解，无需使用 RL。

### 8.2 解耦架构

```
迷宫探索阶段：
    输入：3×3 视野序列
    算法：DRQN / PPO + LSTM
    输出：移动动作序列

          ↓ 进入 BOSS 战

BOSS 战阶段：
    输入：BOSS 血量、技能集合、当前金币数
    算法：分支限界法 / DP
    输出：最优技能序列、复活策略
```

### 8.3 伪代码

```
function BOSS_Battle_Strategy(boss_hp, skills, gold):
    // 分支限界求解最优技能序列
    best_rounds, best_seq ← BranchAndBound_BOSS(boss_hp, skills)

    // 计算复活策略
    revival_cost ← 100  // 每次复活消耗的金币
    max_revivals ← gold // revival_cost

    // 判断是否能击败 BOSS
    if best_rounds ≤ 回合限制:
        return "WIN", best_seq, max_revivals
    else:
        // 尝试使用金币复活
        for revivals = 1 to max_revivals:
            if 可以通过复活击败 BOSS:
                return "WIN_WITH_REVIVAL", best_seq, revivals
        return "LOSE", null, 0
```

---

## 九、创新点

1. **POMDP 建模**：将 3×3 局部视野问题建模为部分可观测 MDP，引入 LSTM 记忆
2. **多算法对比**：系统性比较贪心、Q-Learning、DRQN、PPO 的性能差异
3. **课程学习**：三阶段训练流水线提升泛化性
4. **策略解耦**：迷宫探索与 BOSS 战使用不同算法，各取所长

---

## 十、参考文献

1. Hausknecht, M., & Stone, P. (2015). Deep Recurrent Q-Learning for Partially Observable MDPs. *AAAI 2015*. [arXiv:1507.06527]
2. Schulman, J., et al. (2017). Proximal Policy Optimization Algorithms. *arXiv:1707.06347*.
3. Cobbe, K., et al. (2020). Leveraging Procedural Generation to Benchmark Reinforcement Learning. *ICML 2020*.
4. Sutton, R. S., & Barto, A. G. (2018). *Reinforcement Learning: An Introduction* (2nd ed.). MIT Press.
5. Mnih, V., et al. (2015). Human-level control through deep reinforcement learning. *Nature*, 518(7540), 529-533.
