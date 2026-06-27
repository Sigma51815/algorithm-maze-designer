// 文件职责：BOSS 战分支限界求解。
// 通过状态记忆、冷却约束和剪枝搜索，求出击败所有 BOSS 的最少回合方案。
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
    cooldowns[skillIndex] = skills[skillIndex].cooldown;
}

int optimisticTurnLowerBound(int remainingHealth,
                             const QVector<BossSkill> &skills,
                             const QVector<int> &cooldowns,
                             int maxTurns) {
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
            return turns;
        }
    }
    return maxTurns + 1;
}

} // namespace

// 分支限界核心：在技能冷却和伤害模式约束下搜索最少回合，并用状态记忆避免重复展开。
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
            if (firstLivingBoss(health) < 0) {
                if (depth < bestTurns) {
                    bestTurns = depth;
                    bestSequence = currentSequence;
                }
                return;
            }

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
                    choices.append(i);
                }
            }
            std::sort(choices.begin(), choices.end(), [&](int first, int second) {
                return skills[first].damage > skills[second].damage;
            });

            for (int skillIndex : choices) {
                QVector<int> nextHealth = health;
                QVector<int> nextCooldowns = cooldowns;
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
    QVector<int> health = bossHealth;
    QVector<int> cooldowns(skills.size(), 0);
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
