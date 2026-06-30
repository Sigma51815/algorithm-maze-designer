// 文件职责：BOSS 战分支限界求解。
// 通过状态记忆、冷却约束和剪枝搜索，求出击败所有 BOSS 的最少回合方案。
/**主要功能：
BOSS 战用分支限界。每个状态由 BOSS 剩余血量和技能冷却组成。先用贪心策略得到一个合法技能序列作为上界 bestTurns。
搜索时，每一回合只枚举冷却为 0 的技能作为分支。对每个状态，用剩余总血量除以最大技能伤害，并结合冷却估计乐观下界。
如果当前回合数加下界已经不能优于上界，就剪枝。
同时用 stateKey(health, cooldowns) 记录状态，如果同一状态以前用更少回合到达过，也剪枝。
最后用 verify() 逐回合重放技能序列，验证冷却约束和击败结果。**/
#include "bosssolver.h"

#include "maze.h"

#include <QHash>

#include <algorithm>
#include <functional>
#include <limits>

namespace {

int firstLivingBoss(const QVector<int> &health) {
    for (int i = 0; i < health.size(); ++i) {
        if (health[i] > 0) {
            return i;
        }
    }
    return -1;
}

QString stateKey(const QVector<int> &health, const QVector<int> &cooldowns) {
    // 状态记忆键：剩余血量 + 技能冷却，相同状态只保留更早到达的搜索分支。
    QString key;
    key.reserve((health.size() + cooldowns.size()) * 5);
    for (int value : health) {
        key += QString::number(std::max(0, value)) + QLatin1Char(',');
    }
    key += QLatin1Char('|');
    for (int value : cooldowns) {
        key += QString::number(value) + QLatin1Char(',');
    }
    return key;
}

void applySkill(QVector<int> &health,
                QVector<int> &cooldowns,
                const QVector<BossSkill> &skills,
                int skillIndex,
                DamageOverflowMode damageMode) {
    const int boss = firstLivingBoss(health);
    // 仿真一回合：技能先作用于当前存活 BOSS，溢出模式下多余伤害顺延。
    if (damageMode == DamageOverflowMode::Overflow) {
        int remainingDamage = skills[skillIndex].damage;
        for (int target = boss; target < health.size() && remainingDamage > 0; ++target) {
            const int dealt = std::min(health[target], remainingDamage);
            health[target] -= dealt;
            remainingDamage -= dealt;
        }
    } else {
        health[boss] -= skills[skillIndex].damage;
    }
    for (int &cooldown : cooldowns) {
        cooldown = std::max(0, cooldown - 1);
    }
    // 当前技能重新进入冷却；搜索和 verify 都只允许 cooldown == 0 的技能被选。
    cooldowns[skillIndex] = skills[skillIndex].cooldown;
}

int optimisticTurnLowerBound(int remainingHealth,
                             const QVector<BossSkill> &skills,
                             const QVector<int> &cooldowns,
                             int maxTurns) {
    // 乐观下界：从“剩余血量 / 最大伤害”起步，再估算冷却限制下的最大伤害容量。
    int maxDamage = 0;
    for (const BossSkill &skill : skills) {
        maxDamage = std::max(maxDamage, skill.damage);
    }
    const int simpleBound = (remainingHealth + maxDamage - 1) / maxDamage;
    for (int turns = simpleBound; turns <= maxTurns; ++turns) {
        qint64 damageCapacity = 0;
        for (int i = 0; i < skills.size(); ++i) {
            const int firstAvailableTurn = cooldowns[i];
            if (turns <= firstAvailableTurn) {
                continue;
            }
            const int usableTurns = turns - firstAvailableTurn;
            const int maximumUses = 1 + (usableTurns - 1) / (skills[i].cooldown + 1);
            damageCapacity += static_cast<qint64>(maximumUses) * skills[i].damage;
        }
        damageCapacity = std::min<qint64>(damageCapacity,
                                          static_cast<qint64>(turns) * maxDamage);
        if (damageCapacity >= remainingHealth) {
            return turns;// 说明 turns 回合内有能力击败所有 BOSS，返回乐观下界。
        }
    }
    return maxTurns + 1;
}

} // namespace

// 任务③入口：先用贪心序列给出上界，再用下界估计、状态记忆和冷却约束做分支限界搜索。
BossResult BossSolver::solve(const QVector<int> &bossHealth,
                             const QVector<BossSkill> &skills,
                             DamageOverflowMode damageMode) {
    BossResult result;
    if (bossHealth.isEmpty()) {
        result.solved = true;
        return result;
    }

    bool hasAlwaysAvailableSkill = false;
    for (const BossSkill &skill : skills) {
        if (skill.damage <= 0 || skill.cooldown < 0) {
            return result;
        }
        hasAlwaysAvailableSkill = hasAlwaysAvailableSkill || skill.cooldown == 0;
    }
    if (skills.isEmpty() || !hasAlwaysAvailableSkill) {
        return result;
    }
    for (int health : bossHealth) {
        if (health <= 0) {
            return result;
        }
    }

    QVector<int> greedyHealth = bossHealth;
    QVector<int> greedyCooldowns(skills.size(), 0);
    QVector<int> bestSequence;
    // 初始上界：每回合选当前可用的最高伤害技能，先得到一个合法解。
    //贪心序列不一定最优，但一定是个合法解
    while (firstLivingBoss(greedyHealth) >= 0) {
        int chosen = -1;
        for (int i = 0; i < skills.size(); ++i) {
            if (greedyCooldowns[i] == 0
                && (chosen < 0 || skills[i].damage > skills[chosen].damage)) {
                chosen = i;
            }
        }
        if (chosen < 0) {
            return result;
        }
        bestSequence.append(chosen);
        applySkill(greedyHealth, greedyCooldowns, skills, chosen, damageMode);
    }

    int bestTurns = bestSequence.size();
    QHash<QString, int> bestDepthForState;
    QVector<int> currentSequence;

    std::function<void(QVector<int>, QVector<int>, int)> search =
        [&](QVector<int> health, QVector<int> cooldowns, int depth) {
            ++result.expandedStates;
            // 找到可行解时更新上界 bestTurns 和当前最优技能序列。
            if (firstLivingBoss(health) < 0) {
                if (depth < bestTurns) {
                    bestTurns = depth;
                    bestSequence = currentSequence;
                }
                return;
            }
            //下界剪枝：最乐观也不能优于当前上界，就停止展开
            int remainingHealth = 0;
            for (int value : health) {
                remainingHealth += std::max(0, value);
            }
            const int optimisticTurns = optimisticTurnLowerBound(
                remainingHealth, skills, cooldowns, bestTurns - depth - 1);
            if (depth + optimisticTurns >= bestTurns) {
                ++result.prunedStates;
                return;
            }
            //// 状态剪枝：同一局面如果曾用更少回合到达，当前分支不可能更优。
            const QString key = stateKey(health, cooldowns);
            const auto previous = bestDepthForState.constFind(key);
            if (previous != bestDepthForState.constEnd() && *previous <= depth) {
                ++result.prunedStates;
                return;
            }
            bestDepthForState.insert(key, depth);

            QVector<int> choices;
            for (int i = 0; i < skills.size(); ++i) {
                if (cooldowns[i] == 0) {
                    choices.append(i);// 当前回合可用的技能编号。 
                }
            }
            // 优先试高伤害技能，尽早压低上界，提高后续剪枝效果。
            std::sort(choices.begin(), choices.end(), [&](int first, int second) {
                return skills[first].damage > skills[second].damage;
            });

            for (int skillIndex : choices) {
                // 每个可用技能形成一个分支；递归返回后 removeLast() 完成回溯。
                QVector<int> nextHealth = health;// 复制当前血量状态，避免修改原状态。
                QVector<int> nextCooldowns = cooldowns;// 复制当前冷却状态，避免修改原状态。
                applySkill(nextHealth, nextCooldowns, skills, skillIndex, damageMode);
                currentSequence.append(skillIndex);
                search(std::move(nextHealth), std::move(nextCooldowns), depth + 1);
                currentSequence.removeLast();
            }
        };

    search(bossHealth, QVector<int>(skills.size(), 0), 0);
    result.solved = true;
    result.minimumTurns = bestTurns;
    result.skillSequence = bestSequence;
    return result;
}

bool BossSolver::verify(const QVector<int> &bossHealth,
                        const QVector<BossSkill> &skills,
                        const QVector<int> &sequence,
                        DamageOverflowMode damageMode) {
    // 验证给定技能序列是否能在冷却约束下击败所有 BOSS。
    QVector<int> health = bossHealth;
    QVector<int> cooldowns(skills.size(), 0);
    // 逐回合重放序列，验证技能编号、冷却约束和最终击败结果。
    for (int skillIndex : sequence) {
        if (firstLivingBoss(health) < 0 || skillIndex < 0 || skillIndex >= skills.size()
            || cooldowns[skillIndex] != 0) {
            return false;
        }
        applySkill(health, cooldowns, skills, skillIndex, damageMode);
    }
    return firstLivingBoss(health) < 0;
}

BossFullResult BossSolver::solveWithMaze(const MazeModel &maze,
                                          const QVector<int> &bossHealth,
                                          const QVector<BossSkill> &skills,
                                          int extraTurns,
                                          DamageOverflowMode damageMode) {
    BossFullResult result;
    BossResult basic = solve(bossHealth, skills, damageMode);
    if (!basic.solved) return result;

    result.solved = true;
    result.minimumTurns = basic.minimumTurns;
    result.skillSequence = basic.skillSequence;
    result.expandedStates = basic.expandedStates;
    result.prunedStates = basic.prunedStates;

    ResourcePlan dp = maze.optimalResourceWalk();
    int collectedCoins = 0;
    for (int cell : dp.collectedCells) {
        collectedCoins += std::max(0, maze.resourceAt(cell));
    }
    result.maxCoinsFromDP = collectedCoins;

    const int clampedExtraTurns = std::clamp(extraTurns, 0, 1);
    result.roundLimit = basic.minimumTurns + clampedExtraTurns;
    result.coinConsumption = std::max(1, (collectedCoins + 1) / 2);

    return result;
}
