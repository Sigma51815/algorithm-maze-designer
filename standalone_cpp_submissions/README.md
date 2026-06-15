# Pure C++ maze submissions
这是提交至云平台所需的C++代码所写的代码，是QtC++代码的等价替换
This directory is separate from the Qt application. Each `.cpp` file is a
complete C++17 console program and can be compiled and submitted alone.

| Requirement | Source file | Algorithm |
| --- | --- | --- |
| REQ_0_1 | `REQ_0_1_divide_and_conquer_perfect_maze.cpp` | Recursive divide and conquer |
| REQ_0_2 | `REQ_0_2_backtracking_dfs_perfect_maze.cpp` | Randomized backtracking DFS |
| REQ_0_3 | `REQ_0_3_branch_and_bound_layered_bfs_perfect_maze.cpp` | Branch and bound with strict BFS layers |
| REQ_0_4 | `REQ_0_4_kruskal_minimum_spanning_tree_perfect_maze.cpp` | Randomized Kruskal MST |

Example compilation (MinGW):

```powershell
g++ -std=c++17 -O2 REQ_0_1_divide_and_conquer_perfect_maze.cpp -o maze.exe
```

Each program reads two odd integers `n m` (`5 <= n,m <= 99`) and prints an
`n` by `m` maze. It places exactly one `S`, `E`, and `B`; `B` is adjacent to
`E`; `C` and `T` satisfy the required minimum counts. The traversable cells
are connected and form a tree.
