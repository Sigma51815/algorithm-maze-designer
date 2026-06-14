# AGENTS.md

## What this is

Algorithm course design (算法课设) — a Qt 6 Widgets desktop app that generates perfect mazes via 4 algorithms, solves resource-optimal paths with DP, and finds minimum-turn BOSS skill sequences with branch-and-bound.

**Tech stack**: C++17, Qt 6 Widgets, CMake + Ninja. No external deps beyond Qt.

## Branch: dev-RL-tmchao7

This branch adds **RL-based maze optimization** on top of the base maze designer. Goal: train an RL agent to generate mazes that minimize AI player scores (maximize difficulty while keeping solvability). See `docs/方案甲_迷宫设计.md` §进阶策略 for the PAIRED/regret-based fitness approach.

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
| `docs/任务设计书.md` | Task spec, grading rubric |
| `docs/算法课程设计思路.md` | Algorithm analysis, pseudocode |
| `docs/助教验收与评分操作手册.html` | TA acceptance checklist |
| `docs/方案甲_迷宫设计.md` | Maze design plan |
| `docs/方案乙_AI玩家.md` | AI player plan (not implemented) |

## Gitignore

`build/`, `dist/`, `.idea/`, `.vscode/`, `*.user`, `*.autosave`
