#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

class BacktrackingDfsMaze {
public:
    BacktrackingDfsMaze(int outputRows, int outputColumns)
        : n(outputRows), m(outputColumns), rows((n - 1) / 2), columns((m - 1) / 2),
          graph(rows * columns), grid(n, string(m, '#')), rng(makeSeed()) {}

    vector<string> generate() {
        generateDepthFirst();
        renderTree();
        placeSpecialCells();
        if (!isValid()) throw runtime_error("internal maze validation failed");
        return grid;
    }

private:
    int n, m, rows, columns;
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
    void addEdge(int first, int second) {
        graph[first].push_back(second);
        graph[second].push_back(first);
        edges.push_back({first, second});
    }

    vector<int> unvisitedNeighbors(int cell, const vector<bool>& visited) const {
        const int row = cell / columns, column = cell % columns;
        vector<int> result;
        if (row > 0 && !visited[id(row - 1, column)]) result.push_back(id(row - 1, column));
        if (row + 1 < rows && !visited[id(row + 1, column)]) result.push_back(id(row + 1, column));
        if (column > 0 && !visited[id(row, column - 1)]) result.push_back(id(row, column - 1));
        if (column + 1 < columns && !visited[id(row, column + 1)]) result.push_back(id(row, column + 1));
        return result;
    }

    // Standard-C++ extraction of MazeModel::generateDepthFirst in
    // src/source/maze.cpp. A cell is connected only when first visited.
    // Backtracking changes the
    // search position but never adds a second parent, so the result is a tree.
    void generateDepthFirst() {
        vector<bool> visited(rows * columns, false);
        vector<int> path;
        const int start = 0; // Same initial startCell_ value used by MazeModel::reset.
        visited[start] = true;
        path.push_back(start);

        while (!path.empty()) {
            const int current = path.back();
            vector<int> choices = unvisitedNeighbors(current, visited);
            if (choices.empty()) {
                path.pop_back();
                continue;
            }
            shuffle(choices.begin(), choices.end(), rng);
            const int next = choices.front();
            visited[next] = true;
            addEdge(current, next);
            path.push_back(next);
        }
    }

    void renderTree() {
        for (int row = 0; row < rows; ++row)
            for (int column = 0; column < columns; ++column)
                grid[row * 2 + 1][column * 2 + 1] = '.';
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
            const int current = pending.front(); pending.pop();
            if (distance[current] > distance[farthest]) farthest = current;
            for (int next : graph[current]) {
                if (distance[next] != -1) continue;
                distance[next] = distance[current] + 1;
                if (parent) (*parent)[next] = current;
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
        for (int row = 1; row + 1 < n; ++row)
            for (int column = 1; column + 1 < m; ++column)
                if (grid[row][column] == '.') available.push_back({row, column});
        shuffle(available.begin(), available.end(), rng);
        const int coinCount = n >= 15 && m >= 15 ? 5 : 1;
        const int trapCount = n >= 15 && m >= 15 ? 3 : 1;
        for (int i = 0; i < coinCount; ++i) {
            auto [row, column] = available.back(); available.pop_back();
            grid[row][column] = 'C';
        }
        for (int i = 0; i < trapCount; ++i) {
            auto [row, column] = available.back(); available.pop_back();
            grid[row][column] = 'T';
        }
    }

    bool isValid() const {
        int starts = 0, exits = 0, bosses = 0, coins = 0, traps = 0;
        int passable = 0, edgeCount = 0;
        pair<int, int> start{-1, -1}, exit{-1, -1}, boss{-1, -1};
        for (int row = 0; row < n; ++row) for (int column = 0; column < m; ++column) {
            const char cell = grid[row][column];
            if (cell == '#') continue;
            ++passable;
            if (cell == 'S') { ++starts; start = {row, column}; }
            if (cell == 'E') { ++exits; exit = {row, column}; }
            if (cell == 'B') { ++bosses; boss = {row, column}; }
            if (cell == 'C') ++coins;
            if (cell == 'T') ++traps;
            if (row + 1 < n && grid[row + 1][column] != '#') ++edgeCount;
            if (column + 1 < m && grid[row][column + 1] != '#') ++edgeCount;
        }
        const int requiredCoins = n >= 15 && m >= 15 ? 5 : 1;
        const int requiredTraps = n >= 15 && m >= 15 ? 3 : 1;
        if (starts != 1 || exits != 1 || bosses != 1 || coins < requiredCoins
            || traps < requiredTraps || abs(exit.first - boss.first)
            + abs(exit.second - boss.second) != 1) return false;

        const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
        vector<vector<bool>> visited(n, vector<bool>(m, false));
        queue<pair<int, int>> pending;
        pending.push(start); visited[start.first][start.second] = true;
        int reached = 0;
        while (!pending.empty()) {
            auto [row, column] = pending.front(); pending.pop(); ++reached;
            for (int direction = 0; direction < 4; ++direction) {
                const int nr = row + dr[direction], nc = column + dc[direction];
                if (nr >= 0 && nr < n && nc >= 0 && nc < m && !visited[nr][nc]
                    && grid[nr][nc] != '#') {
                    visited[nr][nc] = true; pending.push({nr, nc});
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
        for (const string& row : BacktrackingDfsMaze(n, m).generate()) cout << row << '\n';
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
