# Relationship with the Qt source code

These files are extractions of the four generation methods in
`src/source/maze.cpp`; they are not four unrelated replacement algorithms.

| Standalone function | Original Qt function |
| --- | --- |
| `generateDivideAndConquer` | `MazeModel::generateDivideAndConquer` |
| `generateDepthFirst` | `MazeModel::generateDepthFirst` |
| `generateBreadthFirst` | `MazeModel::generateBreadthFirst` |
| `generateKruskal` | `MazeModel::generateKruskal` |

## Equivalent type replacements

| Qt project type/API | Standalone C++17 type/API |
| --- | --- |
| `QVector<T>` | `std::vector<T>` |
| `QQueue<T>` | `std::queue<T>` |
| `QString` / `QStringList` | `std::string` / `std::vector<std::string>` |
| `quint32` | `unsigned int` |
| `QVector<bool>` | `std::vector<bool>` |

The generation decisions remain the same: recursive rectangular splitting,
DFS first-visit carving, Kruskal union-find edge selection, and layered BFS
priority expansion with repeated candidates.

## Necessary submission adapters

The following code is outside the original generation method because each
online-judge source file must run independently:

- `main` reads `n m` and prints the result.
- The judge's `n x m` character matrix is converted to logical cells of size
  `(n-1)/2 x (m-1)/2` before running the original algorithm.
- `S`, `E`, and `B` are placed after generation. `B` occupies the corridor
  immediately before `E`, so the two characters are adjacent in the submitted
  matrix as required by the problem statement.
- `C` and `T` use the fixed minimum counts required by this subproblem. The Qt
  application instead receives their counts from UI controls.
- `isValid` is a submission-only final assertion for connectivity and `E=V-1`.

The Qt project under `src/` is unchanged. The standalone files provide the same
algorithmic implementation without requiring Qt libraries or GUI classes.
