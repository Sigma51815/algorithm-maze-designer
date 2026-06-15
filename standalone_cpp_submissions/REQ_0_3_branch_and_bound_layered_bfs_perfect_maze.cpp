#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

class BranchAndBoundMaze {
public:
    BranchAndBoundMaze(int outputRows, int outputColumns)
        : n(outputRows), m(outputColumns), rows((n - 1) / 2), columns((m - 1) / 2),
          graph(rows * columns), grid(n, string(m, '#')), rng(makeSeed()) {}

    vector<string> generate() {
        generateBreadthFirst();
        renderTree();
        placeSpecialCells();
        if (!isValid()) throw runtime_error("internal maze validation failed");
        return grid;
    }

private:
    struct Branch {
        int lowerBound;
        unsigned int tieBreaker;
        int from;
        int to;
        int depth;
        int randomCost;
        int sourceDegree;
    };
    struct WorseBranch {
        bool operator()(const Branch& first, const Branch& second) const {
            if (first.lowerBound != second.lowerBound)
                return first.lowerBound > second.lowerBound;
            return first.tieBreaker > second.tieBreaker;
        }
    };

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
    vector<int> gridNeighbors(int cell) const {
        const int row = cell / columns, column = cell % columns;
        vector<int> result;
        if (row > 0) result.push_back(id(row - 1, column));
        if (row + 1 < rows) result.push_back(id(row + 1, column));
        if (column > 0) result.push_back(id(row, column - 1));
        if (column + 1 < columns) result.push_back(id(row, column + 1));
        return result;
    }

    pair<int, int> candidateScoreAndDiameter(
        const vector<pair<int, int>>& candidate) const {
        vector<vector<int>> candidateGraph(rows * columns);
        for (auto [first, second] : candidate) {
            candidateGraph[first].push_back(second);
            candidateGraph[second].push_back(first);
        }
        auto farthest = [&](int source) {
            vector<int> distance(candidateGraph.size(), -1);
            queue<int> pending;
            pending.push(source); distance[source] = 0;
            int result = source;
            while (!pending.empty()) {
                const int current = pending.front(); pending.pop();
                if (distance[current] > distance[result]) result = current;
                for (int next : candidateGraph[current]) if (distance[next] == -1) {
                    distance[next] = distance[current] + 1;
                    pending.push(next);
                }
            }
            return pair<int, int>{result, distance[result]};
        };
        const int endpoint = farthest(0).first;
        const int diameter = farthest(endpoint).second;
        int deadEnds = 0;
        for (const auto& adjacent : candidateGraph)
            if (adjacent.size() == 1) ++deadEnds;
        return {diameter * 10 + deadEnds, diameter};
    }

    vector<pair<int, int>> buildOneCandidate() {
        const int cellCount = rows * columns;
        vector<bool> visited(cellCount, false);
        vector<int> degree(cellCount, 0);
        vector<pair<int, int>> candidate;
        priority_queue<Branch, vector<Branch>, WorseBranch> frontier;
        uniform_int_distribution<int> randomCost(0, 99);
        uniform_int_distribution<unsigned int> randomTie;

        auto addBranches = [&](int cell, int depth) {
            vector<int> choices = gridNeighbors(cell);
            shuffle(choices.begin(), choices.end(), rng);
            for (int next : choices) {
                if (visited[next]) continue;
                const int cost = randomCost(rng);
                // The depth term dominates all other terms, so branches are
                // expanded in strict BFS layers. Other terms only order one layer.
                const int bound = depth * 10000 + degree[cell] * 100 + cost;
                frontier.push({bound, randomTie(rng), cell, next, depth,
                               cost, degree[cell]});
            }
        };

        const int root = uniform_int_distribution<int>(0, cellCount - 1)(rng);
        visited[root] = true;
        addBranches(root, 1);

        while (!frontier.empty()) {
            Branch branch = frontier.top(); frontier.pop();
            // This is the branch-and-bound pruning rule: a state already
            // reached at an equal or smaller BFS bound cannot receive a second parent.
            if (visited[branch.to]) continue;
            if (branch.sourceDegree != degree[branch.from]) {
                branch.sourceDegree = degree[branch.from];
                branch.lowerBound = branch.depth * 10000
                                  + branch.sourceDegree * 100 + branch.randomCost;
                frontier.push(branch);
                continue;
            }
            visited[branch.to] = true;
            ++degree[branch.from];
            ++degree[branch.to];
            candidate.push_back({branch.from, branch.to});
            addBranches(branch.to, branch.depth + 1);
        }
        return candidate;
    }

    // Standard-C++ extraction of MazeModel::generateBreadthFirst in
    // src/source/maze.cpp: same priority bound, 12 restarts and incumbent score.
    void generateBreadthFirst() {
        int incumbentScore = numeric_limits<int>::min();
        vector<pair<int, int>> incumbent;
        const int directDistance = rows + columns - 2;
        // Random restarts form the outer branch-and-bound search. The best
        // diameter/dead-end score found so far is retained as the incumbent.
        for (int attempt = 0; attempt < 12; ++attempt) {
            vector<pair<int, int>> candidate = buildOneCandidate();
            const auto [score, diameter] = candidateScoreAndDiameter(candidate);
            if (score > incumbentScore) {
                incumbentScore = score;
                incumbent = move(candidate);
            }
            if (diameter >= directDistance + 6) break;
        }
        edges = move(incumbent);
        for (auto [first, second] : edges) {
            graph[first].push_back(second);
            graph[second].push_back(first);
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
        pending.push(source); distance[source] = 0;
        if (parent) { parent->assign(graph.size(), -1); (*parent)[source] = source; }
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
        for (const string& row : BranchAndBoundMaze(n, m).generate()) cout << row << '\n';
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
