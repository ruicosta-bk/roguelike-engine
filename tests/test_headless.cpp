#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <cassert>
#include <cmath>
#include <string>
#include <cstdlib>
#include "map.hpp"
#include "ai.hpp"
#include "engine.hpp"

// Simple testing framework
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << "): " #cond << "\n"; \
            g_tests_failed++; \
            return false; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        g_tests_run++; \
        std::cout << "[RUN ] " << #fn << "..." << std::endl; \
        if (fn()) { \
            std::cout << "[PASS] " << #fn << "\n"; \
            g_tests_passed++; \
        } else { \
            std::cout << "[FAIL] " << #fn << "\n"; \
        } \
    } while (0)

// 1. Map generation tests
bool test_map_generation() {
    const int W = 80;
    const int H = 25;
    Map map(W, H);
    map.generate(1337);

    TEST_ASSERT(map.getWidth() == W, "Map width mismatch");
    TEST_ASSERT(map.getHeight() == H, "Map height mismatch");

    const auto& rooms = map.getRooms();
    TEST_ASSERT(!rooms.empty(), "Rooms vector must not be empty");
    std::cout << "    Generated " << rooms.size() << " rooms.\n";

    Point spawn = map.getPlayerSpawn();
    TEST_ASSERT(map.isInBounds(spawn.x, spawn.y), "Player spawn out of bounds");
    TEST_ASSERT(map.isWalkable(spawn.x, spawn.y), "Player spawn must be walkable");

    // Locate stairs down
    Point stairs{-1, -1};
    bool foundStairs = false;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (map.getTile(x, y) == TileType::StairsDown) {
                stairs = {x, y};
                foundStairs = true;
                break;
            }
        }
        if (foundStairs) break;
    }
    TEST_ASSERT(foundStairs, "StairsDown tile not found on generated map");
    TEST_ASSERT(map.isWalkable(stairs.x, stairs.y), "StairsDown must be walkable");

    // Reachability test using BFS from spawn to stairs
    std::vector<std::vector<bool>> visited(H, std::vector<bool>(W, false));
    std::queue<Point> q;
    q.push(spawn);
    visited[spawn.y][spawn.x] = true;

    bool stairsReachable = false;
    const int dx[4] = {0, 0, 1, -1};
    const int dy[4] = {1, -1, 0, 0};

    while (!q.empty()) {
        Point curr = q.front();
        q.pop();

        if (curr.x == stairs.x && curr.y == stairs.y) {
            stairsReachable = true;
            break;
        }

        for (int i = 0; i < 4; ++i) {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];
            if (map.isInBounds(nx, ny) && !visited[ny][nx] && map.isWalkable(nx, ny)) {
                visited[ny][nx] = true;
                q.push({nx, ny});
            }
        }
    }
    TEST_ASSERT(stairsReachable, "StairsDown must be reachable from player spawn");

    // Check monster spawns
    auto monsterSpawns = map.getMonsterSpawns(5);
    TEST_ASSERT(!monsterSpawns.empty(), "Monster spawns should not be empty");
    for (const auto& m : monsterSpawns) {
        TEST_ASSERT(map.isInBounds(m.x, m.y), "Monster spawn out of bounds");
        TEST_ASSERT(map.isWalkable(m.x, m.y), "Monster spawn must be walkable");
    }

    return true;
}

// 2. A* Pathfinding tests
bool test_pathfinding() {
    Map map(80, 25);
    map.generate(42);

    const auto& rooms = map.getRooms();
    TEST_ASSERT(rooms.size() >= 2, "Need at least 2 rooms for pathfinding test");

    Point start = rooms[0].center();
    Point target = rooms[1].center();

    TEST_ASSERT(map.isWalkable(start.x, start.y), "Start room center must be walkable");
    TEST_ASSERT(map.isWalkable(target.x, target.y), "Target room center must be walkable");

    auto path = Pathfinding::findPath(map, start, target);
    TEST_ASSERT(!path.empty(), "Path between room centers should not be empty");

    // Verify all steps are walkable and contiguous
    Point prev = start;
    for (size_t i = 0; i < path.size(); ++i) {
        Point step = path[i];
        TEST_ASSERT(map.isInBounds(step.x, step.y), "Path step out of bounds");
        TEST_ASSERT(map.isWalkable(step.x, step.y), "Path step must be walkable");

        int dist = std::abs(step.x - prev.x) + std::abs(step.y - prev.y);
        TEST_ASSERT(dist == 1, "Steps must be adjacent (4-way connectivity)");
        prev = step;
    }

    // Last step should reach the target
    TEST_ASSERT(path.back().x == target.x && path.back().y == target.y, "Path must end at target");

    // Test blocker behavior
    if (path.size() > 2) {
        Point blocker = path[0];
        auto pathWithBlocker = Pathfinding::findPath(map, start, target, {blocker});
        if (!pathWithBlocker.empty()) {
            TEST_ASSERT(!(pathWithBlocker[0] == blocker), "Path should not step into blocker tile");
        }
    }

    // Test start == target
    auto selfPath = Pathfinding::findPath(map, start, start);
    TEST_ASSERT(selfPath.empty(), "Path to self should be empty");

    return true;
}

// 3. Bresenham FOV tests
bool test_fov() {
    Map map(80, 25);
    map.generate(999);

    FogOfWar fow;
    Point spawn = map.getPlayerSpawn();

    fow.computeFOV(map, spawn, 8);

    // Spawn tile must be visible and explored
    TEST_ASSERT(fow.isVisible(spawn.x, spawn.y), "Origin must be visible");
    TEST_ASSERT(fow.isExplored(spawn.x, spawn.y), "Origin must be explored");

    // Check visible tiles count
    int visibleCount = 0;
    for (int y = 0; y < map.getHeight(); ++y) {
        for (int x = 0; x < map.getWidth(); ++x) {
            if (fow.isVisible(x, y)) {
                visibleCount++;
                // Check distance
                double d = std::sqrt((x - spawn.x) * (x - spawn.x) + (y - spawn.y) * (y - spawn.y));
                TEST_ASSERT(d <= 9.0, "Visible tile is outside FOV radius");
                TEST_ASSERT(fow.isExplored(x, y), "Visible tile must also be marked explored");
            }
        }
    }
    TEST_ASSERT(visibleCount > 0, "At least some tiles should be visible");
    std::cout << "    Visible tiles around spawn: " << visibleCount << "\n";

    // Test resetVisibility
    fow.resetVisibility();
    TEST_ASSERT(!fow.isVisible(spawn.x, spawn.y), "Spawn should not be visible after resetVisibility");
    TEST_ASSERT(fow.isExplored(spawn.x, spawn.y), "Spawn must remain explored after resetVisibility");

    return true;
}

// 4. 500-tick game simulation
bool test_headless_simulation() {
    std::cout << "    Instantiating Engine and running 500-tick headless simulation...\n";
    Engine engine;
    engine.runHeadless(500, 1337);

    // Run another simulation with different seed to ensure stability
    Engine engine2;
    engine2.runHeadless(500, 7777);

    std::cout << "    Headless simulation completed 500 ticks successfully with 2 distinct seeds.\n";
    return true;
}

int main() {
    std::cout << "========================================\n";
    std::cout << " Terminal Roguelike - Headless Test Suite\n";
    std::cout << "========================================\n";

    RUN_TEST(test_map_generation);
    RUN_TEST(test_pathfinding);
    RUN_TEST(test_fov);
    RUN_TEST(test_headless_simulation);

    std::cout << "========================================\n";
    std::cout << " Tests run:    " << g_tests_run << "\n";
    std::cout << " Tests passed: " << g_tests_passed << "\n";
    std::cout << " Tests failed: " << g_tests_failed << "\n";
    std::cout << "========================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
