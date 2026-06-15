#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

class DivideAndConquerMaze {
public:
    DivideAndConquerMaze(int outputRows, int outputColumns)
        : n(outputRows), m(outputColumns), rows((n - 1) / 2), columns((m - 1) / 2),
          graph(rows * columns), grid(n, string(m, '#')), rng(makeSeed()) {}

    vector<string> generate() {
        generateDivideAndConquer(0, rows - 1, 0, columns - 1);
        renderTree();
        placeSpecialCells();
        if (!isValid()) {
            throw runtime_error("internal maze validation failed");
        }
        return grid;
    }

private:
    int n;
    int m;
    int rows;
    int columns;
    vector<vector<int>> graph;
    vector<pair<int, int>> edges;
    vector<string> grid;
    mt19937 rng;

    static unsigned int makeSeed() {
        return static_cast<unsigned int>(chrono::steady_clock::now().time_since_epoch().count())
             ^ random_device{}();
    }

    int id(int row, int column) const { return row * columns + column; }
    pair<int, int> logicalPosition(int cell) const {
        return {cell / columns, cell % columns};
    }

    int randomInt(int low, int high) {
        return uniform_int_distribution<int>(low, high)(rng);
    }

    void addEdge(int first, int second) {
        graph[first].push_back(second);
        graph[second].push_back(first);
        edges.push_back({first, second});
    }

    // Each recursive call builds a tree for one rectangle. The single edge
    // added between its two child rectangles preserves connectivity and adds no cycle.
    // Standard-C++ extraction of MazeModel::generateDivideAndConquer in
    // src/source/maze.cpp. QVector and quint32 are replaced by STL types only.
    void generateDivideAndConquer(int top, int bottom, int left, int right) {
        if (top == bottom && left == right) {
            return;
        }

        const int height = bottom - top + 1;
        const int width = right - left + 1;
        const bool splitVertically = width > 1 && (height == 1 || width >= height);

        if (splitVertically) {
            const int splitColumn = randomInt(left, right - 1);
            generateDivideAndConquer(top, bottom, left, splitColumn);
            generateDivideAndConquer(top, bottom, splitColumn + 1, right);
            const int openingRow = randomInt(top, bottom);
            addEdge(id(openingRow, splitColumn), id(openingRow, splitColumn + 1));
        } else {
            const int splitRow = randomInt(top, bottom - 1);
            generateDivideAndConquer(top, splitRow, left, right);
            generateDivideAndConquer(splitRow + 1, bottom, left, right);
            const int openingColumn = randomInt(left, right);
            addEdge(id(splitRow, openingColumn), id(splitRow + 1, openingColumn));
        }
    }

    void renderTree() {
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                grid[row * 2 + 1][column * 2 + 1] = '.';
            }
        }
        for (auto [first, second] : edges) {
            auto [r1, c1] = logicalPosition(first);
            auto [r2, c2] = logicalPosition(second);
            grid[r1 + r2 + 1][c1 + c2 + 1] = '.';
        }
    }

    int farthestFrom(int source, vector<int>* parent = nullptr) const {
        vector<int> distance(graph.size(), -1);
        queue<int> pending;
        pending.push(source);
        distance[source] = 0;
        if (parent) {
            parent->assign(graph.size(), -1);
            (*parent)[source] = source;
        }
        int farthest = source;
        while (!pending.empty()) {
            const int current = pending.front();
            pending.pop();
            if (distance[current] > distance[farthest]) {
                farthest = current;
            }
            for (int next : graph[current]) {
                if (distance[next] != -1) {
                    continue;
                }
                distance[next] = distance[current] + 1;
                if (parent) {
                    (*parent)[next] = current;
                }
                pending.push(next);
            }
        }
        return farthest;
    }

    void placeSpecialCells() {
        const int start = farthestFrom(0);
        vector<int> parent;
        const int exit = farthestFrom(start, &parent);
        const int beforeExit = parent[exit];

        auto [sr, sc] = logicalPosition(start);
        auto [er, ec] = logicalPosition(exit);
        auto [br, bc] = logicalPosition(beforeExit);
        grid[sr * 2 + 1][sc * 2 + 1] = 'S';
        grid[er * 2 + 1][ec * 2 + 1] = 'E';
        grid[er + br + 1][ec + bc + 1] = 'B';

        vector<pair<int, int>> available;
        for (int row = 1; row + 1 < n; ++row) {
            for (int column = 1; column + 1 < m; ++column) {
                if (grid[row][column] == '.') {
                    available.push_back({row, column});
                }
            }
        }
        shuffle(available.begin(), available.end(), rng);
        const int coinCount = n >= 15 && m >= 15 ? 5 : 1;
        const int trapCount = n >= 15 && m >= 15 ? 3 : 1;
        for (int i = 0; i < coinCount; ++i) {
            grid[available.back().first][available.back().second] = 'C';
            available.pop_back();
        }
        for (int i = 0; i < trapCount; ++i) {
            grid[available.back().first][available.back().second] = 'T';
            available.pop_back();
        }
    }

    bool isValid() const {
        int starts = 0, exits = 0, bosses = 0, coins = 0, traps = 0;
        int passable = 0, edgeCount = 0;
        pair<int, int> startPosition{-1, -1}, exitPosition{-1, -1}, bossPosition{-1, -1};
        const int dr[4] = {-1, 1, 0, 0};
        const int dc[4] = {0, 0, -1, 1};
        for (int row = 0; row < n; ++row) {
            for (int column = 0; column < m; ++column) {
                const char cell = grid[row][column];
                if (cell == '#') continue;
                ++passable;
                if (cell == 'S') { ++starts; startPosition = {row, column}; }
                if (cell == 'E') { ++exits; exitPosition = {row, column}; }
                if (cell == 'B') { ++bosses; bossPosition = {row, column}; }
                if (cell == 'C') ++coins;
                if (cell == 'T') ++traps;
                if (row + 1 < n && grid[row + 1][column] != '#') ++edgeCount;
                if (column + 1 < m && grid[row][column + 1] != '#') ++edgeCount;
            }
        }
        const int requiredCoins = n >= 15 && m >= 15 ? 5 : 1;
        const int requiredTraps = n >= 15 && m >= 15 ? 3 : 1;
        if (starts != 1 || exits != 1 || bosses != 1 || coins < requiredCoins
            || traps < requiredTraps) return false;
        if (abs(exitPosition.first - bossPosition.first)
            + abs(exitPosition.second - bossPosition.second) != 1) return false;

        vector<vector<bool>> visited(n, vector<bool>(m, false));
        queue<pair<int, int>> pending;
        pending.push(startPosition);
        visited[startPosition.first][startPosition.second] = true;
        int reached = 0;
        while (!pending.empty()) {
            auto [row, column] = pending.front();
            pending.pop();
            ++reached;
            for (int direction = 0; direction < 4; ++direction) {
                const int nr = row + dr[direction], nc = column + dc[direction];
                if (nr >= 0 && nr < n && nc >= 0 && nc < m && !visited[nr][nc]
                    && grid[nr][nc] != '#') {
                    visited[nr][nc] = true;
                    pending.push({nr, nc});
                }
            }
        }
        return reached == passable && edgeCount == passable - 1;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n, m;
    if (!(cin >> n >> m)) return 0;
    if (n < 5 || n > 99 || m < 5 || m > 99 || n % 2 == 0 || m % 2 == 0) {
        cerr << "n and m must be odd integers in [5, 99].\n";
        return 1;
    }

    try {
        for (const string& row : DivideAndConquerMaze(n, m).generate()) {
            cout << row << '\n';
        }
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
