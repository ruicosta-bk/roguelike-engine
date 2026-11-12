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

    // Check item spawns
    auto itemSpawns = map.getItemSpawns(4);
    TEST_ASSERT(!itemSpawns.empty(), "Item spawns should not be empty");
    for (const auto& it : itemSpawns) {
        TEST_ASSERT(map.isInBounds(it.x, it.y), "Item spawn out of bounds");
        TEST_ASSERT(map.isWalkable(it.x, it.y), "Item spawn must be walkable");
        TEST_ASSERT(!(it == spawn), "Item spawn must not collide with player spawn");
        TEST_ASSERT(!(it == stairs), "Item spawn must not collide with stairs down");
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

    FogOfWar fow(80, 25);
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

// 4. Item pickup and healing mechanics
bool test_item_pickup_and_healing() {
    Engine engine;
    engine.init(1234);

    const auto& items = engine.getItems();
    TEST_ASSERT(!items.empty(), "Potions must be spawned on initial floor");
    std::cout << "    Floor 1 spawned " << items.size() << " items.\n";

    // Damage player to test healing
    engine.getPlayer().hp = 10;
    engine.getPlayer().maxHp = 30;

    // Find an open walkable neighbor tile
    Point playerPos = engine.getPlayer().pos;
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    int walkDir = -1;
    for (int i = 0; i < 4; ++i) {
        Point neighbor{playerPos.x + dx[i], playerPos.y + dy[i]};
        if (engine.getMap().isWalkable(neighbor.x, neighbor.y)) {
            // Ensure no monster on that tile
            bool monsterThere = false;
            for (const auto& m : engine.getMonsters()) {
                if (m.isAlive() && m.pos == neighbor) {
                    monsterThere = true;
                    break;
                }
            }
            if (!monsterThere) {
                walkDir = i;
                break;
            }
        }
    }
    TEST_ASSERT(walkDir != -1, "Player must have at least one open adjacent walkable tile");

    Point targetPos{playerPos.x + dx[walkDir], playerPos.y + dy[walkDir]};

    // Place a custom test potion on targetPos
    Item testPotion;
    testPotion.id = 9999;
    testPotion.name = "Test Health Potion";
    testPotion.pos = targetPos;
    testPotion.healAmount = 12;
    testPotion.pickedUp = false;
    engine.getItems().push_back(testPotion);

    // Step player onto targetPos
    bool moved = engine.stepPlayer(dx[walkDir], dy[walkDir]);
    TEST_ASSERT(moved, "stepPlayer onto item tile should succeed");
    TEST_ASSERT(engine.getPlayer().pos == targetPos, "Player should be on target item tile");

    // Verify item is picked up
    bool foundAndPicked = false;
    for (const auto& it : engine.getItems()) {
        if (it.id == 9999) {
            TEST_ASSERT(it.pickedUp, "Item must be marked as pickedUp");
            foundAndPicked = true;
            break;
        }
    }
    TEST_ASSERT(foundAndPicked, "Test potion must exist in engine items list");

    // Verify player healed: 10 + 12 = 22
    TEST_ASSERT(engine.getPlayer().hp == 22, "Player HP should increase by healAmount (10 + 12 = 22)");

    // Test healing cap at maxHp
    engine.getPlayer().hp = 28;
    Item capPotion;
    capPotion.id = 10000;
    capPotion.pos = targetPos; // Already standing on this tile
    capPotion.healAmount = 10;
    capPotion.pickedUp = false;
    engine.getItems().push_back(capPotion);

    engine.checkItemPickup();
    TEST_ASSERT(engine.getPlayer().hp == 30, "Player HP must be capped at maxHp (30)");

    return true;
}

// 5. Safe rest healing over 5 wait turns
bool test_safe_rest_healing() {
    Engine engine;
    engine.init(5555);

    // Clear all monsters so no enemies are in FOV
    engine.getMonsters().clear();

    // Damage player
    engine.getPlayer().hp = 10;
    engine.getPlayer().maxHp = 30;

    // Rest 4 turns: safeRestCount should reach 4, HP remains 10
    for (int i = 0; i < 4; ++i) {
        engine.waitTurn();
    }
    TEST_ASSERT(engine.getSafeRestCount() == 4, "Safe rest count should be 4 after 4 waits");
    TEST_ASSERT(engine.getPlayer().hp == 10, "HP should not heal before 5th rest turn");

    // 5th rest turn: should trigger +1 HP healing
    engine.waitTurn();
    TEST_ASSERT(engine.getPlayer().hp == 11, "HP should heal by 1 on 5th rest turn (10 -> 11)");

    // Rest another 5 turns: should heal to 12
    for (int i = 0; i < 5; ++i) {
        engine.waitTurn();
    }
    TEST_ASSERT(engine.getPlayer().hp == 12, "HP should heal to 12 after 10 safe rest turns");

    // Verify safe rest does not exceed maxHp
    engine.getPlayer().hp = engine.getPlayer().maxHp;
    for (int i = 0; i < 5; ++i) {
        engine.waitTurn();
    }
    TEST_ASSERT(engine.getPlayer().hp == engine.getPlayer().maxHp, "Safe rest must not exceed maxHp");

    return true;
}

// 6. EXP gain and Level Up stats increase
bool test_exp_and_level_up() {
    Engine engine;
    engine.init(777);

    TEST_ASSERT(engine.getPlayerLevel() == 1, "Initial player level should be 1");
    TEST_ASSERT(engine.getPlayerExp() == 0, "Initial player EXP should be 0");
    TEST_ASSERT(engine.getPlayerExpToNext() == 20, "Initial EXP to next should be 20");

    int initialMaxHp = engine.getPlayer().maxHp;
    int initialAtk = engine.getPlayer().attack;
    int initialDef = engine.getPlayer().defense;

    // Add partial EXP
    engine.addExp(10);
    TEST_ASSERT(engine.getPlayerLevel() == 1, "Level should still be 1 with 10/20 EXP");
    TEST_ASSERT(engine.getPlayerExp() == 10, "EXP should be 10");

    // Damage player to verify full heal on level up
    engine.getPlayer().hp = 5;

    // Add 15 EXP -> total 25 EXP >= 20 -> triggers level up
    engine.addExp(15);
    TEST_ASSERT(engine.getPlayerLevel() == 2, "Level should be 2 after level up");
    TEST_ASSERT(engine.getPlayerExp() == 5, "Leftover EXP should be 5 (25 - 20)");
    TEST_ASSERT(engine.getPlayerExpToNext() == 40, "Exp to next level should double to 40");

    // Verify stat increases: +5 maxHp, +1 attack, +1 defense, full heal
    TEST_ASSERT(engine.getPlayer().maxHp == initialMaxHp + 5, "Max HP should increase by 5");
    TEST_ASSERT(engine.getPlayer().hp == engine.getPlayer().maxHp, "HP should be restored to full on level up");
    TEST_ASSERT(engine.getPlayer().attack == initialAtk + 1, "Attack should increase by 1");
    TEST_ASSERT(engine.getPlayer().defense == initialDef + 1, "Defense should increase by 1");

    return true;
}

// 7. Quick restart test
bool test_quick_restart() {
    Engine engine;
    engine.init(888);

    // Advance turns and modify state
    engine.stepPlayer(0, 0); // may do nothing or move
    engine.getPlayer().hp = 5;
    engine.addExp(30);

    // Trigger restart
    engine.restart();

    TEST_ASSERT(engine.isRunning(), "Engine should be running after restart");
    TEST_ASSERT(!engine.isGameOver(), "Engine should not be game over after restart");
    TEST_ASSERT(!engine.isVictory(), "Engine should not be victory after restart");
    TEST_ASSERT(engine.getTurnCount() == 0, "Turn count should reset to 0");
    TEST_ASSERT(engine.getFloor() == 1, "Floor should reset to 1");
    TEST_ASSERT(engine.getPlayerLevel() == 1, "Level should reset to 1");
    TEST_ASSERT(engine.getPlayerExp() == 0, "EXP should reset to 0");
    TEST_ASSERT(engine.getPlayer().hp == engine.getPlayer().maxHp, "HP should reset to maxHp");

    return true;
}

// 8. 500-tick game simulation
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
    RUN_TEST(test_item_pickup_and_healing);
    RUN_TEST(test_safe_rest_healing);
    RUN_TEST(test_exp_and_level_up);
    RUN_TEST(test_quick_restart);
    RUN_TEST(test_headless_simulation);

    std::cout << "========================================\n";
    std::cout << " Tests run:    " << g_tests_run << "\n";
    std::cout << " Tests passed: " << g_tests_passed << "\n";
    std::cout << " Tests failed: " << g_tests_failed << "\n";
    std::cout << "========================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
