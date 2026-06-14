# 算法驱动的迷宫设计

本项目依据 [`docs/2025-2026算法课设任务书.pdf`](docs/2025-2026算法课设任务书.pdf)
中“1. 迷宫设计任务”以及
[`docs/助教验收与评分操作手册.html`](docs/助教验收与评分操作手册.html) 实现，
采用 C++17 和 Qt 6 Widgets，包含完整图形化界面。

## 已实现内容

- 四种迷宫生成：分治、Kruskal 最小生成树、回溯 DFS、严格分层的随机化 BFS 分支限界。
- 生成过程动画，尺寸、随机种子和动画速度可现场修改。
- 完美迷宫自动校验，并显示解路径、死路、分叉点和最长走廊等挑战性指标。
- 自动用树直径两端作为起点和终点，使主线达到当前迷宫的最大长度。
- 挑战性资源布置：部分陷阱位于必经主线，金币优先位于支路，并进行图形化显示。
- 动态规划求最大资源及合法行走路径，资源只在首次经过时计分。
- 分支限界求 BOSS 战最少回合和最优技能序列，并逐回合验证冷却。
- 独立回合制战斗动画窗口，包含角色、血条、伤害特效、技能冷却和最优序列自动播放。
- 输出限定回合数、复活金币数、搜索状态数和剪枝数。
- 导出 JSON：扩展迷宫矩阵、通路、资源、DP 路径和 BOSS 参数。
- `--self-test` 自动测试四种迷宫，并用小规模穷举/BFS 独立核对 DP 与 BOSS 最优答案。
- BFS 形态测试会检查起终点路线和最长连续墙段，防止退化成梳齿状图案。

源码按类型存放：头文件位于 `src/include`，实现文件位于 `src/source`。

## 编译运行

可直接双击 `dist/maze_designer.exe` 运行已打包成品。修改源码后，双击
`运行迷宫设计.bat` 可重新编译并启动。

也可在 PowerShell 中运行：

```powershell
$env:QTFRAMEWORK_BYPASS_LICENSE_CHECK='1'
& 'F:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH='F:\Qt\6.11.1\mingw_64' `
  -DCMAKE_CXX_COMPILER='F:\Qt\Tools\mingw1310_64\bin\g++.exe' `
  -DCMAKE_MAKE_PROGRAM='F:\Qt\Tools\Ninja\ninja.exe'
& 'F:\Qt\Tools\CMake_64\bin\cmake.exe' --build build --parallel
$env:PATH='F:\Qt\6.11.1\mingw_64\bin;F:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
& '.\build\maze_designer.exe'
```

运行自动测试：

```powershell
& '.\build\maze_designer.exe' --self-test
```

## 验收建议

1. 依次选择四种算法，使用相同 `15 x 15` 尺寸和不同随机种子生成迷宫。
2. 展示生成动画，并指出校验信息中的 `V=225、E=224、连通`。
3. 重新布置资源，执行“DP 求最优路径”，展示最大资源和蓝色行走路线。
4. 临时修改金币、陷阱、BOSS 血量或技能参数后重新运行。
5. 执行 BOSS 求解，说明上界、下界、状态记忆和剪枝数。
6. 导出 JSON，展示矩阵、路径与技能序列均可复核。

更完整的算法原理、复杂度和任务对应关系见
[`docs/课程设计说明.md`](docs/课程设计说明.md)。
