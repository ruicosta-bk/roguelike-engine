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

// 4. Inventory pickup and capacity limit
bool test_inventory_pickup_and_capacity() {
    Engine engine;
    engine.init(1001);

    TEST_ASSERT(engine.getInventory().empty(), "Initial inventory must be empty");

    // Find an open walkable neighbor tile
    Point playerPos = engine.getPlayer().pos;
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    int walkDir = -1;
    for (int i = 0; i < 4; ++i) {
        Point neighbor{playerPos.x + dx[i], playerPos.y + dy[i]};
        if (engine.getMap().isWalkable(neighbor.x, neighbor.y)) {
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
    TEST_ASSERT(walkDir != -1, "Player must have an open adjacent tile");

    Point targetPos{playerPos.x + dx[walkDir], playerPos.y + dy[walkDir]};

    // Place a test item on targetPos
    Item testItem;
    testItem.id = 8801;
    testItem.name = "Test Dagger";
    testItem.category = ItemCategory::Weapon;
    testItem.pos = targetPos;
    testItem.atkBonus = 3;
    testItem.pickedUp = false;
    engine.getItems().push_back(testItem);

    // Step player onto item
    bool moved = engine.stepPlayer(dx[walkDir], dy[walkDir]);
    TEST_ASSERT(moved, "Player move should succeed");
    TEST_ASSERT(engine.getInventory().size() == 1, "Item should be added to inventory");
    TEST_ASSERT(engine.getInventory()[0].id == 8801, "Picked up item ID should match");

    // Now test capacity limit (MAX_INVENTORY_CAPACITY = 10)
    for (int i = 2; i <= 10; ++i) {
        Item dummy;
        dummy.id = 8800 + i;
        dummy.name = "Dummy Item " + std::to_string(i);
        dummy.category = ItemCategory::Potion;
        dummy.healAmount = 5;
        dummy.pickedUp = true;
        engine.getInventory().push_back(dummy);
    }
    TEST_ASSERT(engine.getInventory().size() == Engine::MAX_INVENTORY_CAPACITY, "Inventory should be at max capacity (10)");

    // Place an 11th item directly on player pos
    Item overflowItem;
    overflowItem.id = 9999;
    overflowItem.name = "Overflow Item";
    overflowItem.pos = engine.getPlayer().pos;
    overflowItem.pickedUp = false;
    engine.getItems().push_back(overflowItem);

    // Attempt pickup
    engine.checkItemPickup();

    // Verify item was NOT picked up
    TEST_ASSERT(engine.getInventory().size() == Engine::MAX_INVENTORY_CAPACITY, "Inventory size must not exceed capacity");
    bool foundUnpicked = false;
    for (const auto& it : engine.getItems()) {
        if (it.id == 9999) {
            TEST_ASSERT(!it.pickedUp, "Overflow item must remain unpicked on the floor");
            foundUnpicked = true;
            break;
        }
    }
    TEST_ASSERT(foundUnpicked, "Overflow item should remain in items list");

    return true;
}

// 5. Equipment stat bonuses
bool test_equipment_stat_bonuses() {
    Engine engine;
    engine.init(2002);

    int baseAtk = engine.getPlayer().attack;
    int baseDef = engine.getPlayer().defense;

    TEST_ASSERT(engine.getTotalPlayerAttack() == baseAtk, "Initial attack should equal base attack");
    TEST_ASSERT(engine.getTotalPlayerDefense() == baseDef, "Initial defense should equal base defense");
    TEST_ASSERT(engine.getEquippedWeapon() == nullptr, "Initial equipped weapon should be null");
    TEST_ASSERT(engine.getEquippedArmor() == nullptr, "Initial equipped armor should be null");

    // Add Weapon 1: Longsword (+5 Atk)
    Item longsword;
    longsword.id = 101;
    longsword.name = "Iron Longsword";
    longsword.category = ItemCategory::Weapon;
    longsword.atkBonus = 5;
    longsword.equipped = false;
    engine.getInventory().push_back(longsword);

    // Equip Longsword
    bool equippedWep = engine.useOrEquipItem(0);
    TEST_ASSERT(equippedWep, "Equipping longsword should return true");
    TEST_ASSERT(engine.getInventory()[0].equipped, "Longsword should be marked equipped");
    TEST_ASSERT(engine.getTotalPlayerAttack() == baseAtk + 5, "Total attack should include +5 bonus");
    TEST_ASSERT(engine.getEquippedWeapon() != nullptr, "Equipped weapon pointer must not be null");

    // Add Armor: Plate Armor (+6 Def)
    Item plate;
    plate.id = 102;
    plate.name = "Plate Armor";
    plate.category = ItemCategory::Armor;
    plate.defBonus = 6;
    plate.equipped = false;
    engine.getInventory().push_back(plate);

    // Equip Plate Armor
    bool equippedArm = engine.useOrEquipItem(1);
    TEST_ASSERT(equippedArm, "Equipping armor should return true");
    TEST_ASSERT(engine.getInventory()[1].equipped, "Plate Armor should be marked equipped");
    TEST_ASSERT(engine.getTotalPlayerDefense() == baseDef + 6, "Total defense should include +6 bonus");
    TEST_ASSERT(engine.getEquippedArmor() != nullptr, "Equipped armor pointer must not be null");

    // Add Weapon 2: Battle Axe (+7 Atk) to test weapon swapping
    Item axe;
    axe.id = 103;
    axe.name = "Battle Axe";
    axe.category = ItemCategory::Weapon;
    axe.atkBonus = 7;
    axe.equipped = false;
    engine.getInventory().push_back(axe);

    // Equip Battle Axe (index 2)
    engine.useOrEquipItem(2);
    TEST_ASSERT(!engine.getInventory()[0].equipped, "Longsword must be automatically unequipped");
    TEST_ASSERT(engine.getInventory()[2].equipped, "Battle Axe must now be equipped");
    TEST_ASSERT(engine.getTotalPlayerAttack() == baseAtk + 7, "Total attack should now reflect +7 Battle Axe");

    // Unequip Battle Axe
    engine.useOrEquipItem(2);
    TEST_ASSERT(!engine.getInventory()[2].equipped, "Battle Axe should now be unequipped");
    TEST_ASSERT(engine.getTotalPlayerAttack() == baseAtk, "Total attack should return to base attack");

    return true;
}

// 6. Potion consumption and healing
bool test_potion_consumption() {
    Engine engine;
    engine.init(3003);

    // Damage player
    engine.getPlayer().hp = 10;
    engine.getPlayer().maxHp = 30;

    // Add Health Potion (+15 HP)
    Item potion;
    potion.id = 201;
    potion.name = "Health Potion";
    potion.category = ItemCategory::Potion;
    potion.healAmount = 15;
    engine.getInventory().push_back(potion);

    // Drink potion (index 0)
    bool used = engine.useOrEquipItem(0);
    TEST_ASSERT(used, "Drinking potion should return true");
    TEST_ASSERT(engine.getPlayer().hp == 25, "Player HP should heal to 25 (10 + 15)");
    TEST_ASSERT(engine.getInventory().empty(), "Consumed potion must be removed from inventory");

    // Add Elixir (+25 HP) to test maxHp clamp
    Item elixir;
    elixir.id = 202;
    elixir.name = "Elixir";
    elixir.category = ItemCategory::Potion;
    elixir.healAmount = 25;
    engine.getInventory().push_back(elixir);

    used = engine.useOrEquipItem(0);
    TEST_ASSERT(used, "Drinking elixir should return true");
    TEST_ASSERT(engine.getPlayer().hp == engine.getPlayer().maxHp, "Player HP must cap at maxHp (30)");
    TEST_ASSERT(engine.getInventory().empty(), "Consumed elixir must be removed from inventory");

    return true;
}

// 7. Scroll of Teleportation
bool test_scroll_of_teleportation() {
    Engine engine;
    engine.init(4004);

    // Add Scroll of Teleportation
    Item scroll;
    scroll.id = 301;
    scroll.name = "Scroll of Teleportation";
    scroll.category = ItemCategory::Scroll;
    engine.getInventory().push_back(scroll);

    // Read scroll
    bool used = engine.useOrEquipItem(0);
    TEST_ASSERT(used, "Reading scroll should return true");
    TEST_ASSERT(engine.getInventory().empty(), "Consumed scroll must be removed from inventory");

    Point newPos = engine.getPlayer().pos;
    TEST_ASSERT(engine.getMap().isInBounds(newPos.x, newPos.y), "Teleported pos must be in bounds");
    TEST_ASSERT(engine.getMap().isWalkable(newPos.x, newPos.y), "Teleported pos must be walkable");

    // Origin fog of war at new pos must be visible
    TEST_ASSERT(engine.getFogOfWar().isVisible(newPos.x, newPos.y), "Teleported position must be visible");

    return true;
}

// 8. Dropping items from inventory
bool test_drop_item() {
    Engine engine;
    engine.init(5005);

    int baseAtk = engine.getPlayer().attack;

    // Add Weapon and equip it
    Item sword;
    sword.id = 401;
    sword.name = "Steel Sword";
    sword.category = ItemCategory::Weapon;
    sword.atkBonus = 4;
    sword.equipped = true;
    engine.getInventory().push_back(sword);

    TEST_ASSERT(engine.getTotalPlayerAttack() == baseAtk + 4, "Equipped sword should add attack bonus");

    // Drop item from inventory (index 0)
    bool dropped = engine.dropItem(0);
    TEST_ASSERT(dropped, "dropItem should return true");
    TEST_ASSERT(engine.getInventory().empty(), "Dropped item must be removed from inventory");
    TEST_ASSERT(engine.getTotalPlayerAttack() == baseAtk, "Attack bonus should be removed when item is dropped");

    // Verify item was placed on the floor at player's position
    bool foundOnFloor = false;
    for (const auto& it : engine.getItems()) {
        if (it.id == 401) {
            TEST_ASSERT(it.pos == engine.getPlayer().pos, "Dropped item must be placed at player pos");
            TEST_ASSERT(!it.pickedUp, "Dropped item must have pickedUp = false");
            TEST_ASSERT(!it.equipped, "Dropped item must have equipped = false");
            foundOnFloor = true;
            break;
        }
    }
    TEST_ASSERT(foundOnFloor, "Dropped item must be present on the dungeon floor");

    return true;
}

// 9. Safe rest healing over 5 wait turns
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

// 10. EXP gain and Level Up stats increase
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

// 11. Quick restart test
bool test_quick_restart() {
    Engine engine;
    engine.init(888);

    // Advance turns and modify state
    engine.stepPlayer(0, 0);
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
    TEST_ASSERT(engine.getInventory().empty(), "Inventory should reset to empty");

    return true;
}

// 12. Skeleton Archer Ranged AI behavior
bool test_skeleton_archer_ranged_ai() {
    Map map(80, 25);
    map.generate(1234);

    const auto& rooms = map.getRooms();
    TEST_ASSERT(!rooms.empty(), "Rooms must not be empty");
    Point center = rooms[0].center();

    // Verify center and offset tiles are walkable
    Point playerPos = center;
    Point archerPos{center.x + 3, center.y}; // distance 3: between min 2 and max 5

    // Ensure floor tiles between player and archer
    for (int x = center.x; x <= archerPos.x; ++x) {
        map.setTile(x, center.y, TileType::Floor);
    }

    // At distance 3 with clear LOS, Archer should choose RangedAttack
    auto actionRanged = MonsterAI::decideAction(
        map, archerPos, playerPos, {}, 8,
        MonsterArchetype::Ranged, 12, 12, 2, 5
    );
    TEST_ASSERT(actionRanged.type == AIActionType::RangedAttack, "Archer at distance 3 with clear LOS should choose RangedAttack");
    TEST_ASSERT(actionRanged.targetPos == playerPos, "Archer ranged attack target must be playerPos");

    // At distance 1 (adjacent <= minRangedDist = 2), Archer should retreat / kite
    Point closeArcherPos{center.x + 1, center.y};
    auto actionClose = MonsterAI::decideAction(
        map, closeArcherPos, playerPos, {}, 8,
        MonsterArchetype::Ranged, 12, 12, 2, 5
    );
    TEST_ASSERT(actionClose.type == AIActionType::Move, "Archer within minRangedDist must attempt to retreat");
    int initialDist = std::abs(closeArcherPos.x - playerPos.x) + std::abs(closeArcherPos.y - playerPos.y);
    int newDist = std::abs(actionClose.targetPos.x - playerPos.x) + std::abs(actionClose.targetPos.y - playerPos.y);
    TEST_ASSERT(newDist >= initialDist, "Archer retreat step must not move closer to player");

    return true;
}

// 13. Goblin Rogue Skirmisher retreat
bool test_goblin_rogue_skirmisher_retreat() {
    Map map(80, 25);
    map.generate(5678);

    Point playerPos = map.getPlayerSpawn();
    Point roguePos{playerPos.x + 2, playerPos.y};
    for (int x = playerPos.x; x <= roguePos.x + 2; ++x) {
        map.setTile(x, playerPos.y, TileType::Floor);
    }

    // Full HP (10/10 > 50%): Skirmisher pursues / attacks
    auto actionHealthy = MonsterAI::decideAction(
        map, roguePos, playerPos, {}, 8,
        MonsterArchetype::Skirmisher, 10, 10
    );
    TEST_ASSERT(actionHealthy.type == AIActionType::Move, "Healthy skirmisher should pursue target");
    int healthyStepDist = std::abs(actionHealthy.targetPos.x - playerPos.x) + std::abs(actionHealthy.targetPos.y - playerPos.y);
    int initialDist = std::abs(roguePos.x - playerPos.x) + std::abs(roguePos.y - playerPos.y);
    TEST_ASSERT(healthyStepDist <= initialDist, "Healthy skirmisher should step closer towards target");

    // Injured HP (4/10 <= 50%): Skirmisher retreats
    auto actionInjured = MonsterAI::decideAction(
        map, roguePos, playerPos, {}, 8,
        MonsterArchetype::Skirmisher, 4, 10
    );
    TEST_ASSERT(actionInjured.type == AIActionType::Move, "Injured skirmisher should flee");
    int fleeStepDist = std::abs(actionInjured.targetPos.x - playerPos.x) + std::abs(actionInjured.targetPos.y - playerPos.y);
    TEST_ASSERT(fleeStepDist >= initialDist, "Injured skirmisher must step away from target");

    return true;
}

// 14. Player Ranged Targeting & Shooting
bool test_player_ranged_combat() {
    Engine engine;
    engine.init(9009);

    const auto& rooms = engine.getMap().getRooms();
    TEST_ASSERT(rooms.size() >= 2, "Need at least 2 rooms for test");

    // Position player in the center of the first room
    Point center = rooms[0].center();
    engine.getPlayer().pos = center;
    engine.getMonsters().clear();

    // Spawn a target dummy 2 tiles to the right (inside the same room, guaranteed walkable with clear LOS)
    Point mPos{center.x + 2, center.y};
    if (!engine.getMap().isWalkable(mPos.x, mPos.y)) {
        mPos = Point{center.x, center.y + 2};
    }
    TEST_ASSERT(engine.getMap().isWalkable(mPos.x, mPos.y), "Target dummy position must be walkable");

    Entity testMonster;
    testMonster.id = 777;
    testMonster.name = "Target Dummy";
    testMonster.pos = mPos;
    testMonster.hp = 20;
    testMonster.maxHp = 20;
    testMonster.defense = 1;
    engine.getMonsters().push_back(testMonster);

    int playerAtk = engine.getTotalPlayerAttack();
    int expectedDmg = std::max(1, playerAtk - testMonster.defense);

    // Shoot at target dummy within range with clear LOS
    bool shot = engine.shootAt(mPos);
    TEST_ASSERT(shot, "Ranged shot at monster within range with clear LOS should succeed");

    // Verify monster took damage
    bool foundMonster = false;
    for (const auto& m : engine.getMonsters()) {
        if (m.id == 777) {
            TEST_ASSERT(m.hp == 20 - expectedDmg, "Monster HP should decrease by damage amount");
            foundMonster = true;
            break;
        }
    }
    TEST_ASSERT(foundMonster, "Target dummy must be in monster list");

    // Test LOS blocked by wall (a target inside room 1 from room 0 is blocked)
    Point room1Center = rooms[1].center();
    Entity wallMonster;
    wallMonster.id = 778;
    wallMonster.name = "Hidden Monster";
    wallMonster.pos = room1Center;
    wallMonster.hp = 20;
    wallMonster.maxHp = 20;
    wallMonster.defense = 1;
    engine.getMonsters().push_back(wallMonster);

    if (!LineOfSight::hasLineOfSight(engine.getMap(), center, room1Center)) {
        bool shotBlocked = engine.shootAt(room1Center);
        TEST_ASSERT(!shotBlocked, "Shot should fail when line of sight is obstructed by walls");
    }

    // Test out of range (> 6 tiles)
    Point farPos{center.x, center.y + 8};
    bool shotFar = engine.shootAt(farPos);
    TEST_ASSERT(!shotFar, "Shot beyond max range 6 must fail");

    return true;
}

// 15. 500-tick game simulation
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
    RUN_TEST(test_inventory_pickup_and_capacity);
    RUN_TEST(test_equipment_stat_bonuses);
    RUN_TEST(test_potion_consumption);
    RUN_TEST(test_scroll_of_teleportation);
    RUN_TEST(test_drop_item);
    RUN_TEST(test_safe_rest_healing);
    RUN_TEST(test_exp_and_level_up);
    RUN_TEST(test_quick_restart);
    RUN_TEST(test_skeleton_archer_ranged_ai);
    RUN_TEST(test_goblin_rogue_skirmisher_retreat);
    RUN_TEST(test_player_ranged_combat);
    RUN_TEST(test_headless_simulation);

    std::cout << "========================================\n";
    std::cout << " Tests run:    " << g_tests_run << "\n";
    std::cout << " Tests passed: " << g_tests_passed << "\n";
    std::cout << " Tests failed: " << g_tests_failed << "\n";
    std::cout << "========================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
