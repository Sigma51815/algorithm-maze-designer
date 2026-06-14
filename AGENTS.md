# AGENTS.md

## What this is

Algorithm course design (算法课设) — a Qt 6 Widgets desktop app that generates perfect mazes via 4 algorithms, solves resource-optimal paths with DP, and finds minimum-turn BOSS skill sequences with branch-and-bound.

**Tech stack**: C++17, Qt 6 Widgets, CMake + Ninja. No external deps beyond Qt.

## Branch: tmchao7-maze-dev

Development branch for personal maze design work. Three-phase plan:

1. **Phase 1 — 基础功能** (done): 4 algo maze gen, DP resource walk, BOSS solver, GUI, JSON export, self-test.
2. **Phase 2 — AI 玩家**: greedy + DP agent with 3×3 local vision to test maze difficulty. Score = `remaining_resource / total_steps`. See `docs/方案乙_AI玩家.md`.
3. **Phase 3 — RL/遗传算法**: PAIRED-based maze optimization — maximize regret (DP optimal vs greedy agent). See `docs/方案甲_迷宫设计.md` §五.

## Task spec (任务设计书)

The authoritative requirements are in `docs/任务设计书.md`. Key grading points:

- **迷宫设计**: 4 algorithms with visualization, connectivity validation (`E = V - 1`), optimal resource path via DP, BOSS min-turns via branch-and-bound.
- **AI 玩家**: greedy resource pickup with 3×3 vision, full maze exploration, score = `remaining_resource / steps`.
- **交叉测试**: maze designers provide "best maze", AI players compete on all mazes — maze score = AI failure rate / average score.

## Build & run

Build script is `运行迷宫设计.bat` (Windows, uses Qt's bundled MinGW + CMake + Ninja). The CMakeLists.txt is **not checked in** — it lives in `build/` (gitignored) or is generated. If you need to create one, target `maze_designer` linking Qt6::Widgets.

```powershell
# Manual build (adjust paths to local Qt install)
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH='<QT_DIR>/6.x.x/<kit>' \
  -DCMAKE_CXX_COMPILER='<QT_DIR>/Tools/mingw*/bin/g++.exe' \
  -DCMAKE_MAKE_PROGRAM='<QT_DIR>/Tools/Ninja/ninja.exe'
cmake --build build --parallel
```

Run self-test (no GUI, CI-friendly):
```
./build/maze_designer.exe --self-test
```

GUI smoke test (opens window, quits after 200ms):
```
./build/maze_designer.exe --gui-smoke-test
```

## Source layout

All source in flat `src/` — no subdirectories.

| File | Role |
|---|---|
| `main.cpp` | Entry point, `--self-test` harness, GUI launch |
| `maze.h/cpp` | `MazeModel` — generation (4 algos), validation, DP resource walk, JSON export |
| `mainwindow.h/cpp` | `MainWindow` — full GUI: controls, animation, export |
| `mazewidget.h/cpp` | `MazeWidget` — QPainter-based maze rendering |
| `bosssolver.h/cpp` | `BossSolver` — branch-and-bound BOSS fight solver |

## Key types

- `MazeModel` — owns the grid, passages (`QVector<array<bool,4>>`), resources, generation steps. Cell = `int` index, row-major.
- `MazeAlgorithm` — enum: `DivideAndConquer`, `KruskalMst`, `DepthFirstSearch`, `BreadthFirstSearch`
- `ResourcePlan` — DP result: `maxValue`, `walk` (cell sequence), `collectedCells`
- `BossResult` — solver output: `minimumTurns`, `skillSequence`, `expandedStates`, `prunedStates`
- `BossSkill` — name, damage, cooldown

## Algorithm details

- **Maze gen**: all 4 produce perfect mazes (tree spanning all cells). `validatePerfect()` checks `E = V - 1` + connectivity.
- **Resource DP**: walks from start (cell 0) to end (cell N-1), collecting coin (+50) / trap (-30) values. Resources count only on first visit.
- **BOSS solver**: branch-and-bound with upper/lower bounds, state memoization, cooldown-aware skill scheduling. `verify()` independently checks sequences.

## Conventions

- Qt types everywhere (`QVector`, `QString`, `QJsonObject`) — no STL containers except `std::array` and `std::mt19937`.
- `#pragma once` headers, no include guards.
- All classes use Qt's `Q_OBJECT` macro where needed for signals/slots.
- Random seeds are `quint32` — deterministic generation for reproducibility.
- JSON export via `QJsonObject` from `MazeModel::toJson()`.

## Docs

| File | Contents |
|---|---|
| `docs/任务设计书.md` | Task spec, grading rubric — **read this first** |
| `docs/算法课程设计思路.md` | Algorithm analysis, pseudocode |
| `docs/助教验收与评分操作手册.html` | TA acceptance checklist |
| `docs/方案甲_迷宫设计.md` | Maze design plan + PAIRED/genetic optimizer (Phase 3) |
| `docs/方案乙_AI玩家.md` | AI player plan: greedy, Q-Learning, DRQN (Phase 2) |

## Gitignore

`build/`, `dist/`, `.idea/`, `.vscode/`, `*.user`, `*.autosave`
