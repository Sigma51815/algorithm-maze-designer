# CLAUDE.md

See `AGENTS.md` for full project context.

## Quick reference

- **Stack**: C++17, Qt 6 Widgets, CMake + Unix Makefiles
- **Branch**: `tmchao7-maze-dev`（最新开发分支）
- **Source**: `src/include/` (headers) + `src/source/` (impls), with `ai/` subdirs
- **Build**: `make` / `make test` / `make run` (macOS)
- **Test**: `./build/maze_designer --self-test`（28 个测试）
- **Headless**: `./build/maze_designer --run-optimizer --rows 7 --cols 7 --enable-rl 1`
- **Key files**: `maze_optimizer.h/cpp`（GA）, `maze_evaluator.h/cpp`（适应度）, `resource_placer.h/cpp`（智能资源分布）, `ai/rl_player.h/cpp`（RL）, `coevolution.h/cpp`（协同进化）
- **Key docs**: `docs/方案甲_迷宫设计.md`（迷宫设计 spec）, `SERVER_GUIDE.md`（服务器部署）
