#include "engine.hpp"

#include <iostream>
#include <algorithm>
#include <random>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

static void sleepMs(int ms) {
#ifdef _WIN32
    Sleep(static_cast<DWORD>(ms));
#else
    usleep(ms * 1000);
#endif
}

Engine::Engine() {
    init(1337);
}

Engine::~Engine() {
    restoreConsole();
}

void Engine::init(unsigned int seed) {
    m_seed = seed;
    m_floor = 1;
    m_turnCount = 0;
    m_safeRestCount = 0;
    m_playerLevel = 1;
    m_playerExp = 0;
    m_playerExpToNext = 20;
    m_running = true;
    m_gameOver = false;
    m_victory = false;
    m_inventoryOpen = false;
    m_targetingMode = false;
    m_targetCursor = {0, 0};
    m_messageLog.clear();
    m_inventory.clear();

    m_map.generate(m_seed);
    m_fow.init(m_map.getWidth(), m_map.getHeight());

    m_player.id = 0;
    m_player.name = "Player";
    m_player.symbol = '@';
    m_player.colorCode = "\033[1;33m"; // Bright yellow
    m_player.pos = m_map.getPlayerSpawn();
    m_player.hp = 30;
    m_player.maxHp = 30;
    m_player.attack = 5;
    m_player.defense = 2;

    addMessage("Welcome to the Dungeons of Doom! [WASD: Move, I: Inventory, Q: Quit]");
    spawnMonsters(6);
    spawnItems(6);
    m_fow.computeFOV(m_map, m_player.pos, 8);
    checkItemPickup();
}

void Engine::restart() {
    init(m_seed + 1);
    addMessage("Game restarted!");
}

void Engine::spawnMonsters(int count) {
    m_monsters.clear();
    auto spawnPoints = m_map.getMonsterSpawns(count);
    int idCounter = 1;
    std::mt19937 rng(m_seed + static_cast<unsigned int>(m_floor) * 1000 + 42);

    for (const auto& pt : spawnPoints) {
        Entity monster;
        monster.id = idCounter++;
        monster.pos = pt;

        unsigned int roll = rng() % 100;
        if (roll < 40) {
            monster.name = "Goblin Rogue";
            monster.symbol = 'g';
            monster.colorCode = "\033[1;32m"; // Bright green
            monster.archetype = MonsterArchetype::Skirmisher;
            monster.hp = 10 + (m_floor - 1) * 2;
            monster.maxHp = monster.hp;
            monster.attack = 3 + (m_floor - 1);
            monster.defense = 1;
        } else if (roll < 75) {
            monster.name = "Skeleton Archer";
            monster.symbol = 'a';
            monster.colorCode = "\033[1;37m"; // Bright white
            monster.archetype = MonsterArchetype::Ranged;
            monster.hp = 12 + (m_floor - 1) * 2;
            monster.maxHp = monster.hp;
            monster.attack = 4 + (m_floor - 1);
            monster.defense = 1;
        } else if (roll < 90) {
            monster.name = "Orc Warrior";
            monster.symbol = 'o';
            monster.colorCode = "\033[1;31m"; // Bright red
            monster.archetype = MonsterArchetype::Melee;
            monster.hp = 18 + (m_floor - 1) * 3;
            monster.maxHp = monster.hp;
            monster.attack = 5 + (m_floor - 1);
            monster.defense = 2;
        } else {
            monster.name = "Dragon";
            monster.symbol = 'D';
            monster.colorCode = "\033[1;35m"; // Bright magenta
            monster.archetype = MonsterArchetype::Melee;
            monster.hp = 35 + (m_floor - 1) * 4;
            monster.maxHp = monster.hp;
            monster.attack = 8 + (m_floor - 1);
            monster.defense = 4;
        }
        m_monsters.push_back(monster);
    }
}

void Engine::spawnItems(int count) {
    m_items.clear();
    auto spawnPoints = m_map.getItemSpawns(count);
    int idCounter = 1;
    std::mt19937 rng(m_seed + static_cast<unsigned int>(m_floor) * 2000 + 77);

    for (size_t i = 0; i < spawnPoints.size(); ++i) {
        Point pt = spawnPoints[i];
        Item item;
        item.id = idCounter++;
        item.pos = pt;
        item.pickedUp = false;
        item.equipped = false;

        unsigned int roll = rng() % 100;

        if (i == 0) {
            // Guaranteed Weapon slot
            if (m_floor == 1) {
                item.name = "Dagger";
                item.category = ItemCategory::Weapon;
                item.symbol = ')';
                item.colorCode = "\033[1;36m";
                item.atkBonus = 3;
            } else if (m_floor == 2) {
                item.name = "Iron Longsword";
                item.category = ItemCategory::Weapon;
                item.symbol = ')';
                item.colorCode = "\033[1;36m";
                item.atkBonus = 5;
            } else {
                item.name = "Battle Axe";
                item.category = ItemCategory::Weapon;
                item.symbol = ')';
                item.colorCode = "\033[1;36m";
                item.atkBonus = 7;
            }
        } else if (i == 1) {
            // Guaranteed Armor slot
            if (m_floor == 1) {
                item.name = "Leather Armor";
                item.category = ItemCategory::Armor;
                item.symbol = '[';
                item.colorCode = "\033[1;37m";
                item.defBonus = 2;
            } else if (m_floor == 2) {
                item.name = "Chainmail";
                item.category = ItemCategory::Armor;
                item.symbol = '[';
                item.colorCode = "\033[1;37m";
                item.defBonus = 4;
            } else {
                item.name = "Plate Armor";
                item.category = ItemCategory::Armor;
                item.symbol = '[';
                item.colorCode = "\033[1;37m";
                item.defBonus = 6;
            }
        } else if (i == 2) {
            // Guaranteed Scroll slot
            item.name = "Scroll of Teleportation";
            item.category = ItemCategory::Scroll;
            item.symbol = '?';
            item.colorCode = "\033[1;33m";
        } else {
            // Potions or additional equipment
            if (roll < 55) {
                item.name = "Health Potion";
                item.category = ItemCategory::Potion;
                item.symbol = '!';
                item.colorCode = "\033[1;35m";
                item.healAmount = 15;
            } else if (roll < 80) {
                item.name = "Elixir";
                item.category = ItemCategory::Potion;
                item.symbol = '!';
                item.colorCode = "\033[1;35m";
                item.healAmount = 25;
            } else {
                item.name = (m_floor >= 2) ? "Iron Longsword" : "Dagger";
                item.category = ItemCategory::Weapon;
                item.symbol = ')';
                item.colorCode = "\033[1;36m";
                item.atkBonus = (m_floor >= 2) ? 5 : 3;
            }
        }
        m_items.push_back(item);
    }
}

int Engine::getTotalPlayerAttack() const {
    int bonus = 0;
    for (const auto& item : m_inventory) {
        if (item.equipped && item.category == ItemCategory::Weapon) {
            bonus += item.atkBonus;
        }
    }
    return m_player.attack + bonus;
}

int Engine::getTotalPlayerDefense() const {
    int bonus = 0;
    for (const auto& item : m_inventory) {
        if (item.equipped && item.category == ItemCategory::Armor) {
            bonus += item.defBonus;
        }
    }
    return m_player.defense + bonus;
}

const Item* Engine::getEquippedWeapon() const {
    for (const auto& item : m_inventory) {
        if (item.equipped && item.category == ItemCategory::Weapon) {
            return &item;
        }
    }
    return nullptr;
}

const Item* Engine::getEquippedArmor() const {
    for (const auto& item : m_inventory) {
        if (item.equipped && item.category == ItemCategory::Armor) {
            return &item;
        }
    }
    return nullptr;
}

void Engine::addMessage(const std::string& msg) {
    m_messageLog.push_back(msg);
    if (m_messageLog.size() > 5) {
        m_messageLog.erase(m_messageLog.begin());
    }
}

void Engine::addExp(int amount) {
    m_playerExp += amount;
    addMessage("You gained " + std::to_string(amount) + " EXP!");
    while (m_playerExp >= m_playerExpToNext) {
        m_playerExp -= m_playerExpToNext;
        m_playerLevel++;
        m_playerExpToNext *= 2;
        m_player.maxHp += 5;
        m_player.hp = m_player.maxHp;
        m_player.attack += 1;
        m_player.defense += 1;
        addMessage("LEVEL UP! You reached Level " + std::to_string(m_playerLevel) + "! (HP restored, Stats +1)");
    }
}

void Engine::checkItemPickup() {
    for (auto& item : m_items) {
        if (!item.pickedUp && item.pos == m_player.pos) {
            if (m_inventory.size() < MAX_INVENTORY_CAPACITY) {
                item.pickedUp = true;
                m_inventory.push_back(item);
                addMessage("Picked up " + item.name + ". Press 'I' to inspect.");
            } else {
                addMessage("Inventory full (10/10)! Cannot pick up " + item.name + ".");
            }
        }
    }
}

bool Engine::useOrEquipItem(size_t index) {
    if (index >= m_inventory.size()) {
        return false;
    }

    Item& item = m_inventory[index];

    if (item.category == ItemCategory::Weapon) {
        if (item.equipped) {
            item.equipped = false;
            addMessage("Unequipped " + item.name + ".");
        } else {
            // Unequip any existing weapon
            for (auto& it : m_inventory) {
                if (it.category == ItemCategory::Weapon) {
                    it.equipped = false;
                }
            }
            item.equipped = true;
            addMessage("Equipped " + item.name + " (+" + std::to_string(item.atkBonus) + " Atk)!");
        }
        return true;
    }

    if (item.category == ItemCategory::Armor) {
        if (item.equipped) {
            item.equipped = false;
            addMessage("Unequipped " + item.name + ".");
        } else {
            // Unequip any existing armor
            for (auto& it : m_inventory) {
                if (it.category == ItemCategory::Armor) {
                    it.equipped = false;
                }
            }
            item.equipped = true;
            addMessage("Equipped " + item.name + " (+" + std::to_string(item.defBonus) + " Def)!");
        }
        return true;
    }

    if (item.category == ItemCategory::Potion) {
        m_player.hp = std::min(m_player.maxHp, m_player.hp + item.healAmount);
        addMessage("You drank " + item.name + " (+" + std::to_string(item.healAmount) + " HP)!");
        m_inventory.erase(m_inventory.begin() + index);
        if (m_inventorySelectedIndex >= m_inventory.size() && !m_inventory.empty()) {
            m_inventorySelectedIndex = m_inventory.size() - 1;
        }
        return true;
    }

    if (item.category == ItemCategory::Scroll) {
        std::vector<Point> walkableTiles;
        for (int y = 0; y < m_map.getHeight(); ++y) {
            for (int x = 0; x < m_map.getWidth(); ++x) {
                if (m_map.isWalkable(x, y)) {
                    bool occupied = (m_player.pos == Point{x, y});
                    for (const auto& m : m_monsters) {
                        if (m.isAlive() && m.pos == Point{x, y}) {
                            occupied = true;
                            break;
                        }
                    }
                    if (!occupied) {
                        walkableTiles.push_back({x, y});
                    }
                }
            }
        }
        if (!walkableTiles.empty()) {
            std::mt19937 rng(m_seed + static_cast<unsigned int>(m_turnCount) * 17 + 101);
            Point dest = walkableTiles[rng() % walkableTiles.size()];
            m_player.pos = dest;
            m_fow.computeFOV(m_map, m_player.pos, 8);
            checkItemPickup();
            addMessage("You read " + item.name + " and teleported away!");
        }
        m_inventory.erase(m_inventory.begin() + index);
        if (m_inventorySelectedIndex >= m_inventory.size() && !m_inventory.empty()) {
            m_inventorySelectedIndex = m_inventory.size() - 1;
        }
        return true;
    }

    return false;
}

bool Engine::dropItem(size_t index) {
    if (index >= m_inventory.size()) {
        return false;
    }
    Item item = m_inventory[index];
    item.equipped = false;
    item.pickedUp = false;
    item.pos = m_player.pos;
    m_items.push_back(item);
    m_inventory.erase(m_inventory.begin() + index);
    if (m_inventorySelectedIndex >= m_inventory.size() && !m_inventory.empty()) {
        m_inventorySelectedIndex = m_inventory.size() - 1;
    }
    addMessage("Dropped " + item.name + " on the floor.");
    return true;
}

void Engine::setInventorySelectedIndex(size_t idx) {
    if (m_inventory.empty()) {
        m_inventorySelectedIndex = 0;
    } else {
        m_inventorySelectedIndex = std::min(idx, m_inventory.size() - 1);
    }
}

void Engine::selectPrevInventoryItem() {
    if (m_inventory.empty()) return;
    if (m_inventorySelectedIndex > 0) {
        m_inventorySelectedIndex--;
    } else {
        m_inventorySelectedIndex = m_inventory.size() - 1;
    }
}

void Engine::selectNextInventoryItem() {
    if (m_inventory.empty()) return;
    if (m_inventorySelectedIndex + 1 < m_inventory.size()) {
        m_inventorySelectedIndex++;
    } else {
        m_inventorySelectedIndex = 0;
    }
}

bool Engine::useOrEquipSelectedItem() {
    if (m_inventory.empty()) return false;
    if (m_inventorySelectedIndex >= m_inventory.size()) {
        m_inventorySelectedIndex = m_inventory.size() - 1;
    }
    bool res = useOrEquipItem(m_inventorySelectedIndex);
    if (m_inventorySelectedIndex >= m_inventory.size() && !m_inventory.empty()) {
        m_inventorySelectedIndex = m_inventory.size() - 1;
    }
    return res;
}

bool Engine::dropSelectedItem() {
    if (m_inventory.empty()) return false;
    if (m_inventorySelectedIndex >= m_inventory.size()) {
        m_inventorySelectedIndex = m_inventory.size() - 1;
    }
    return dropItem(m_inventorySelectedIndex);
}

void Engine::handleInventoryClick(int mouseX, int mouseY, bool rightClick) {
    if (!m_inventoryOpen || m_inventory.empty()) return;

    const int modalW = 560;
    const int modalX = (1120 - modalW) / 2;
    const int modalY = 20;
    const int itemStartY = modalY + 45;
    const int rowH = 24;

    int count = static_cast<int>(m_inventory.size());
    int itemEndY = itemStartY + count * rowH;

    if (mouseX >= modalX + 15 && mouseX <= modalX + modalW - 15) {
        if (mouseY >= itemStartY && mouseY < itemEndY) {
            size_t clicked = static_cast<size_t>((mouseY - itemStartY) / rowH);
            if (clicked < m_inventory.size()) {
                m_inventorySelectedIndex = clicked;
                if (rightClick) {
                    dropItem(clicked);
                } else {
                    useOrEquipItem(clicked);
                }
                if (m_inventorySelectedIndex >= m_inventory.size() && !m_inventory.empty()) {
                    m_inventorySelectedIndex = m_inventory.size() - 1;
                }
            }
        }
    }
}

void Engine::attackEntity(Entity& attacker, Entity& defender) {
    int atkVal = (attacker.id == m_player.id) ? getTotalPlayerAttack() : attacker.attack;
    int defVal = (defender.id == m_player.id) ? getTotalPlayerDefense() : defender.defense;

    int damage = std::max(1, atkVal - defVal);
    defender.hp -= damage;
    if (defender.hp <= 0) {
        defender.hp = 0;
        addMessage(attacker.name + " hits " + defender.name + " for " + std::to_string(damage) + " dmg (KILLED)!");

        if (defender.id == m_player.id) {
            m_gameOver = true;
            m_running = false;
            addMessage("You have died! GAME OVER.");
        } else if (attacker.id == m_player.id) {
            // Player defeated monster: grant exp
            int expVal = 10;
            if (defender.name == "Goblin Rogue" || defender.name == "Goblin") expVal = 10;
            else if (defender.name == "Skeleton Archer" || defender.name == "Skeleton") expVal = 15;
            else if (defender.name == "Orc Warrior" || defender.name == "Orc") expVal = 25;
            else if (defender.name == "Dragon") expVal = 50;
            addExp(expVal);
        }
    } else {
        addMessage(attacker.name + " hits " + defender.name + " for " + std::to_string(damage) + " dmg (" +
                   std::to_string(defender.hp) + "/" + std::to_string(defender.maxHp) + " HP).");
    }
}

bool Engine::stepPlayer(int dx, int dy) {
    if (m_gameOver || m_victory) return false;

    Point target{m_player.pos.x + dx, m_player.pos.y + dy};
    if (!m_map.isInBounds(target.x, target.y)) {
        return false;
    }

    // Moving resets safe rest count
    m_safeRestCount = 0;

    // Bump combat: check if an alive monster is at target tile
    for (auto& monster : m_monsters) {
        if (monster.isAlive() && monster.pos == target) {
            attackEntity(m_player, monster);
            updateMonsters();
            m_turnCount++;
            m_fow.computeFOV(m_map, m_player.pos, 8);
            return true;
        }
    }

    // Closed door interaction: open door on bump
    if (m_map.getTile(target.x, target.y) == TileType::DoorClosed) {
        m_map.setTile(target.x, target.y, TileType::DoorOpen);
        addMessage("You opened the door.");
        updateMonsters();
        m_turnCount++;
        m_fow.computeFOV(m_map, m_player.pos, 8);
        return true;
    }

    // Walkable floor or open door or stairs
    if (m_map.isWalkable(target.x, target.y)) {
        m_player.pos = target;
        checkItemPickup();

        if (m_map.getTile(target.x, target.y) == TileType::StairsDown) {
            addMessage("You see stairs leading down here. Press '>' to descend.");
        }
        updateMonsters();
        m_turnCount++;
        m_fow.computeFOV(m_map, m_player.pos, 8);
        return true;
    }

    addMessage("The way is blocked.");
    return false;
}

bool Engine::descendStairs() {
    if (m_map.getTile(m_player.pos.x, m_player.pos.y) == TileType::StairsDown) {
        if (m_floor >= 3) {
            m_victory = true;
            m_running = false;
            addMessage("You retrieved the Amulet of Yendor and escaped the dungeon! VICTORY!");
            return true;
        }
        m_floor++;
        m_safeRestCount = 0;
        m_map.generate(m_seed + static_cast<unsigned int>(m_floor) * 100);
        m_fow.init(m_map.getWidth(), m_map.getHeight());
        m_player.pos = m_map.getPlayerSpawn();
        m_player.hp = std::min(m_player.maxHp, m_player.hp + 8); // Descending heals some HP
        spawnMonsters(6 + m_floor);
        spawnItems(6);
        m_fow.computeFOV(m_map, m_player.pos, 8);
        checkItemPickup();
        addMessage("You descended to Dungeon Floor " + std::to_string(m_floor) + "!");
        m_turnCount++;
        return true;
    } else {
        addMessage("There are no stairs down here.");
        return false;
    }
}

void Engine::waitTurn() {
    if (m_gameOver || m_victory) return;

    // Natural Healing (Safe Rest):
    // Check if any alive monster is currently visible in FOV
    bool monsterVisible = false;
    for (const auto& m : m_monsters) {
        if (m.isAlive() && m_fow.isVisible(m.pos.x, m.pos.y)) {
            monsterVisible = true;
            break;
        }
    }

    if (!monsterVisible) {
        m_safeRestCount++;
        if (m_safeRestCount >= 5) {
            m_safeRestCount = 0;
            if (m_player.hp < m_player.maxHp) {
                m_player.hp = std::min(m_player.maxHp, m_player.hp + 1);
                addMessage("You rest safely and recover 1 HP.");
            } else {
                addMessage("You rest safely (HP full).");
            }
        } else {
            addMessage("You wait a turn.");
        }
    } else {
        m_safeRestCount = 0;
        addMessage("You wait a turn (Enemies near!).");
    }

    updateMonsters();
    m_turnCount++;
    m_fow.computeFOV(m_map, m_player.pos, 8);
}

void Engine::updateMonsters() {
    if (m_gameOver || m_victory) return;

    for (auto& monster : m_monsters) {
        if (!monster.isAlive()) continue;

        // Collect positions of other alive monsters as pathfinding blockers
        std::vector<Point> blockers;
        for (const auto& other : m_monsters) {
            if (other.isAlive() && other.id != monster.id) {
                blockers.push_back(other.pos);
            }
        }

        AIAction action = MonsterAI::decideAction(
            m_map, monster.pos, m_player.pos, blockers, 8,
            monster.archetype, monster.hp, monster.maxHp, 2, 5
        );

        if (action.type == AIActionType::Attack) {
            attackEntity(monster, m_player);
            if (!m_player.isAlive()) {
                m_gameOver = true;
                m_running = false;
                break;
            }
        } else if (action.type == AIActionType::RangedAttack) {
            int damage = std::max(1, monster.attack - getTotalPlayerDefense());
            m_player.hp -= damage;
            addMessage(monster.name + " shoots an arrow at Player for " + std::to_string(damage) + " dmg!");
            if (m_player.hp <= 0) {
                m_player.hp = 0;
                m_gameOver = true;
                m_running = false;
                addMessage("You were killed by ranged fire! GAME OVER.");
                break;
            }
        } else if (action.type == AIActionType::Move) {
            Point nextPos = action.targetPos;
            if (m_map.isWalkable(nextPos.x, nextPos.y) && nextPos != m_player.pos) {
                bool blocked = false;
                for (const auto& other : m_monsters) {
                    if (other.isAlive() && other.id != monster.id && other.pos == nextPos) {
                        blocked = true;
                        break;
                    }
                }
                if (!blocked) {
                    monster.pos = nextPos;
                }
            }
        }
    }
}

void Engine::toggleTargetingMode() {
    if (m_targetingMode) {
        m_targetingMode = false;
        addMessage("Targeting cancelled.");
        return;
    }
    m_targetingMode = true;

    // Auto-lock onto nearest visible alive monster within range 6
    Point bestTarget{-1, -1};
    int bestDist = 999;
    for (const auto& m : m_monsters) {
        if (m.isAlive() && m_fow.isVisible(m.pos.x, m.pos.y)) {
            int d = std::max(std::abs(m.pos.x - m_player.pos.x), std::abs(m.pos.y - m_player.pos.y));
            if (d <= 6 && d < bestDist && LineOfSight::hasLineOfSight(m_map, m_player.pos, m.pos)) {
                bestDist = d;
                bestTarget = m.pos;
            }
        }
    }

    if (bestTarget.x != -1) {
        m_targetCursor = bestTarget;
    } else {
        // Fallback: 2 tiles in front or player pos
        m_targetCursor = { m_player.pos.x + 2, m_player.pos.y };
        if (!m_map.isInBounds(m_targetCursor.x, m_targetCursor.y)) {
            m_targetCursor = m_player.pos;
        }
    }
    addMessage("Targeting mode active. [WASD/Arrows] Aim | [Enter/Space/F] Shoot | [ESC] Cancel");
}

void Engine::moveTargetCursor(int dx, int dy) {
    Point next{ m_targetCursor.x + dx, m_targetCursor.y + dy };
    if (m_map.isInBounds(next.x, next.y)) {
        m_targetCursor = next;
    }
}

bool Engine::shootAtTarget() {
    if (!m_map.isInBounds(m_targetCursor.x, m_targetCursor.y)) {
        addMessage("Target out of bounds.");
        return false;
    }

    int dist = std::max(std::abs(m_targetCursor.x - m_player.pos.x), std::abs(m_targetCursor.y - m_player.pos.y));
    if (dist > 6) {
        addMessage("Target is out of ranged weapon range (max 6 tiles).");
        return false;
    }

    if (!LineOfSight::hasLineOfSight(m_map, m_player.pos, m_targetCursor)) {
        addMessage("Line of sight to target is blocked.");
        return false;
    }

    // Check if monster at target
    Entity* targetMonster = nullptr;
    for (auto& m : m_monsters) {
        if (m.isAlive() && m.pos == m_targetCursor) {
            targetMonster = &m;
            break;
        }
    }

    if (!targetMonster) {
        addMessage("No monster at target position.");
        return false;
    }

    // Inflict ranged attack damage
    int dmg = std::max(1, getTotalPlayerAttack() - targetMonster->defense);
    targetMonster->hp -= dmg;
    if (targetMonster->hp <= 0) {
        targetMonster->hp = 0;
        addMessage("You shoot " + targetMonster->name + " with an arrow for " + std::to_string(dmg) + " dmg (KILLED)!");
        int expVal = 10;
        if (targetMonster->name == "Goblin Rogue" || targetMonster->name == "Goblin") expVal = 10;
        else if (targetMonster->name == "Skeleton Archer" || targetMonster->name == "Skeleton") expVal = 15;
        else if (targetMonster->name == "Orc Warrior" || targetMonster->name == "Orc") expVal = 25;
        else if (targetMonster->name == "Dragon") expVal = 50;
        addExp(expVal);
    } else {
        addMessage("You shoot " + targetMonster->name + " with an arrow for " + std::to_string(dmg) + " dmg (" +
                   std::to_string(targetMonster->hp) + "/" + std::to_string(targetMonster->maxHp) + " HP).");
    }

    m_targetingMode = false;
    m_turnCount++;
    updateMonsters();
    m_fow.computeFOV(m_map, m_player.pos, 8);
    return true;
}

bool Engine::shootAt(Point target) {
    m_targetCursor = target;
    return shootAtTarget();
}

std::string Engine::renderToString() const {
    std::string out;
    out.reserve(80 * 25 * 4 + 2048);

    // Cursor reset to top-left to avoid flicker
    out += "\033[H";

    int width = m_map.getWidth();
    int height = m_map.getHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool visible = m_fow.isVisible(x, y);
            bool explored = m_fow.isExplored(x, y);

            if (m_targetingMode && m_targetCursor.x == x && m_targetCursor.y == y) {
                out += "\033[1;36m*";
                continue;
            }

            if (visible) {
                if (m_player.pos.x == x && m_player.pos.y == y) {
                    out += m_player.colorCode;
                    out += m_player.symbol;
                    continue;
                }

                bool monsterDrawn = false;
                for (const auto& m : m_monsters) {
                    if (m.isAlive() && m.pos.x == x && m.pos.y == y) {
                        out += m.colorCode;
                        out += m.symbol;
                        monsterDrawn = true;
                        break;
                    }
                }
                if (monsterDrawn) continue;

                // Check consumable item or equipment on visible tile
                bool itemDrawn = false;
                for (const auto& item : m_items) {
                    if (!item.pickedUp && item.pos.x == x && item.pos.y == y) {
                        out += item.colorCode;
                        out += item.symbol;
                        itemDrawn = true;
                        break;
                    }
                }
                if (itemDrawn) continue;

                TileType t = m_map.getTile(x, y);
                switch (t) {
                    case TileType::Wall:
                        out += "\033[1;37m#"; // Bright White
                        break;
                    case TileType::Floor:
                        out += "\033[0;37m."; // Soft White
                        break;
                    case TileType::DoorClosed:
                        out += "\033[1;33m+"; // Bright Yellow
                        break;
                    case TileType::DoorOpen:
                        out += "\033[0;33m/"; // Yellow
                        break;
                    case TileType::StairsDown:
                        out += "\033[1;36m>"; // Bright Cyan
                        break;
                }
            } else if (explored) {
                out += "\033[90m"; // Dim grey
                TileType t = m_map.getTile(x, y);
                switch (t) {
                    case TileType::Wall:
                        out += '#';
                        break;
                    case TileType::Floor:
                        out += '.';
                        break;
                    case TileType::DoorClosed:
                        out += '+';
                        break;
                    case TileType::DoorOpen:
                        out += '/';
                        break;
                    case TileType::StairsDown:
                        out += '>';
                        break;
                }
            } else {
                out += ' ';
            }
        }
        out += "\033[0m\n";
    }

    // HUD Divider
    out += "\033[1;34m" + std::string(static_cast<size_t>(width), '=') + "\033[0m\n";

    // HUD Line 1: Floor, Turn, Level, EXP
    out += " Floor: \033[1;36m" + std::to_string(m_floor) + "\033[0m | ";
    out += "Turn: \033[1;36m" + std::to_string(m_turnCount) + "\033[0m | ";
    out += "Level: \033[1;32m" + std::to_string(m_playerLevel) + "\033[0m | ";
    out += "EXP: \033[1;33m" + std::to_string(m_playerExp) + "/" + std::to_string(m_playerExpToNext) + "\033[0m\033[K\n";

    // HUD Line 2: HP Bar, Atk (with weapon bonus), Def (with armor bonus)
    int barWidth = 16;
    int curHp = std::max(0, m_player.hp);
    int maxHp = std::max(1, m_player.maxHp);
    int filled = (curHp * barWidth) / maxHp;
    filled = std::max(0, std::min(barWidth, filled));

    std::string hpColor = "\033[1;32m"; // Green
    if (curHp <= maxHp / 4) {
        hpColor = "\033[1;31m"; // Red
    } else if (curHp <= maxHp / 2) {
        hpColor = "\033[1;33m"; // Yellow
    }

    const Item* wep = getEquippedWeapon();
    const Item* arm = getEquippedArmor();
    std::string wepStr = wep ? (" (+" + std::to_string(wep->atkBonus) + " " + wep->name + ")") : "";
    std::string armStr = arm ? (" (+" + std::to_string(arm->defBonus) + " " + arm->name + ")") : "";

    out += " HP: " + hpColor + "[" + std::string(static_cast<size_t>(filled), '=') +
           std::string(static_cast<size_t>(barWidth - filled), ' ') + "] ";
    out += std::to_string(curHp) + "/" + std::to_string(maxHp) + "\033[0m | ";
    out += "Atk: \033[1;35m" + std::to_string(getTotalPlayerAttack()) + wepStr + "\033[0m | ";
    out += "Def: \033[1;35m" + std::to_string(getTotalPlayerDefense()) + armStr + "\033[0m\033[K\n";

    // Status / Controls
    if (m_gameOver) {
        out += " \033[1;31m*** GAME OVER - Press [R] to Restart, [Q] to Quit ***\033[0m\033[K\n";
    } else if (m_victory) {
        out += " \033[1;32m*** VICTORY! Dungeon Cleared! Press [R] to Restart, [Q] to Quit ***\033[0m\033[K\n";
    } else if (m_targetingMode) {
        out += " \033[1;36m*** TARGETING MODE *** [WASD] Aim | [Enter/Space/F] Shoot | [ESC] Cancel\033[0m\033[K\n";
    } else if (m_inventoryOpen) {
        out += " \033[1;33m*** INVENTORY OPEN *** [W/S] Select | [Enter/Space] Use | [D] Drop | [I] Close\033[0m\033[K\n";
    } else {
        out += " \033[90mControls: [WASD] Move | [F] Shoot | [I] Inventory | [.] Rest | [>] Stairs | [R] Restart | [Q] Quit\033[0m\033[K\n";
    }

    // Modal or Combat Log
    if (m_inventoryOpen) {
        out += "\033[1;33m--- Inventory (" + std::to_string(m_inventory.size()) + "/10) ---\033[0m\033[K\n";
        for (size_t i = 0; i < 5; ++i) {
            if (i < m_inventory.size()) {
                const auto& item = m_inventory[i];
                bool isSelected = (i == m_inventorySelectedIndex);
                std::string prefix = isSelected ? " > " : "   ";
                std::string tag = item.equipped ? " \033[1;32m[EQUIPPED]\033[0m" : "";
                std::string bonus = "";
                if (item.category == ItemCategory::Weapon) bonus = " (+ " + std::to_string(item.atkBonus) + " Atk)";
                else if (item.category == ItemCategory::Armor) bonus = " (+ " + std::to_string(item.defBonus) + " Def)";
                else if (item.category == ItemCategory::Potion) bonus = " (Heal " + std::to_string(item.healAmount) + " HP)";
                else if (item.category == ItemCategory::Scroll) bonus = " (Teleport)";

                std::string col = isSelected ? "\033[1;37m" : "\033[0;37m";
                out += prefix + col + "[" + std::to_string(i + 1) + "] " + item.name + bonus + tag + "\033[0m\033[K\n";
            } else {
                out += "\033[K\n";
            }
        }
    } else {
        out += "\033[1;33m--- Combat Log ---\033[0m\033[K\n";
        int msgCount = static_cast<int>(m_messageLog.size());
        int startIdx = std::max(0, msgCount - 5);
        int printed = 0;
        for (int i = startIdx; i < msgCount; ++i) {
            out += " > " + m_messageLog[static_cast<size_t>(i)] + "\033[K\n";
            printed++;
        }
        for (int i = printed; i < 5; ++i) {
            out += "\033[K\n";
        }
    }

    out += "\033[0m";
    return out;
}

void Engine::render() const {
    std::string frame = renderToString();
    std::cout << frame << std::flush;
}

void Engine::initConsole() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            m_originalConsoleMode = dwMode;
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "\033[2J\033[H\033[?25l" << std::flush; // Clear screen and hide cursor
}

void Engine::restoreConsole() {
    std::cout << "\033[?25h\033[0m" << std::flush; // Restore cursor and text colors
#ifdef _WIN32
    if (m_originalConsoleMode != 0) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            SetConsoleMode(hOut, m_originalConsoleMode);
        }
    }
#endif
}

void Engine::handleKey(int ch) {
    if (m_inventoryOpen) {
        if (ch == 'i' || ch == 'I' || ch == 27) {
            m_inventoryOpen = false;
        } else if (ch == 'w' || ch == 'W' || ch == 'k' || ch == 'K') {
            selectPrevInventoryItem();
        } else if (ch == 's' || ch == 'S' || ch == 'j' || ch == 'J') {
            selectNextInventoryItem();
        } else if (ch == 13 || ch == ' ') {
            useOrEquipSelectedItem();
        } else if (ch == 'd' || ch == 'D') {
            dropSelectedItem();
        } else if (ch >= '1' && ch <= '9') {
            size_t idx = static_cast<size_t>(ch - '1');
            setInventorySelectedIndex(idx);
            useOrEquipItem(idx);
        } else if (ch == '0') {
            setInventorySelectedIndex(9);
            useOrEquipItem(9);
        } else if (ch >= 'a' && ch <= 'j') {
            size_t idx = static_cast<size_t>(ch - 'a');
            setInventorySelectedIndex(idx);
            useOrEquipItem(idx);
        } else if (ch >= 'A' && ch <= 'J') {
            size_t idx = static_cast<size_t>(ch - 'A');
            setInventorySelectedIndex(idx);
            useOrEquipItem(idx);
        } else if (ch == '!') {
            dropItem(0);
        } else if (ch == '@') {
            dropItem(1);
        } else if (ch == '#') {
            dropItem(2);
        } else if (ch == '$') {
            dropItem(3);
        } else if (ch == '%') {
            dropItem(4);
        } else if (ch == '^') {
            dropItem(5);
        } else if (ch == '&') {
            dropItem(6);
        } else if (ch == '*') {
            dropItem(7);
        } else if (ch == '(') {
            dropItem(8);
        } else if (ch == ')') {
            dropItem(9);
        }
        return;
    }

    if (m_targetingMode) {
        if (ch == 27 || ch == 'q' || ch == 'Q') {
            m_targetingMode = false;
            addMessage("Targeting cancelled.");
        } else if (ch == 'f' || ch == 'F' || ch == 13 || ch == ' ') {
            shootAtTarget();
        } else if (ch == 'w' || ch == 'W' || ch == 'k' || ch == 'K') {
            moveTargetCursor(0, -1);
        } else if (ch == 's' || ch == 'S' || ch == 'j' || ch == 'J') {
            moveTargetCursor(0, 1);
        } else if (ch == 'a' || ch == 'A' || ch == 'h' || ch == 'H') {
            moveTargetCursor(-1, 0);
        } else if (ch == 'd' || ch == 'D' || ch == 'l' || ch == 'L') {
            moveTargetCursor(1, 0);
        }
        return;
    }

    switch (ch) {
        case 'q':
        case 'Q':
        case 27: // ESC
            m_running = false;
            break;
        case 'r':
        case 'R':
            restart();
            break;
        case 'i':
        case 'I':
            m_inventoryOpen = true;
            break;
        case 'f':
        case 'F':
            toggleTargetingMode();
            break;
        case 'w':
        case 'W':
        case 'k':
        case 'K':
            stepPlayer(0, -1);
            break;
        case 's':
        case 'S':
        case 'j':
        case 'J':
            stepPlayer(0, 1);
            break;
        case 'a':
        case 'A':
        case 'h':
        case 'H':
            stepPlayer(-1, 0);
            break;
        case 'd':
        case 'D':
        case 'l':
        case 'L':
            stepPlayer(1, 0);
            break;
        case '.':
        case ' ':
            waitTurn();
            break;
        case '>':
            descendStairs();
            break;
        default:
            break;
    }
}

void Engine::handleSpecialKey(int code) {
    if (m_inventoryOpen) {
        if (code == 72) { // Up
            selectPrevInventoryItem();
        } else if (code == 80) { // Down
            selectNextInventoryItem();
        }
        return;
    }
    if (m_targetingMode) {
        if (code == 72) moveTargetCursor(0, -1);
        else if (code == 80) moveTargetCursor(0, 1);
        else if (code == 75) moveTargetCursor(-1, 0);
        else if (code == 77) moveTargetCursor(1, 0);
        return;
    }
    switch (code) {
        case 72: // Up
            stepPlayer(0, -1);
            break;
        case 80: // Down
            stepPlayer(0, 1);
            break;
        case 75: // Left
            stepPlayer(-1, 0);
            break;
        case 77: // Right
            stepPlayer(1, 0);
            break;
        default:
            break;
    }
}

void Engine::runInteractive() {
    initConsole();
    render();

    while (m_running) {
        if (m_gameOver || m_victory) {
            render();
            while (m_gameOver || m_victory) {
#ifdef _WIN32
                if (_kbhit()) {
                    int ch = _getch();
                    if (ch == 'q' || ch == 'Q' || ch == 27) {
                        m_running = false;
                        break;
                    } else if (ch == 'r' || ch == 'R') {
                        restart();
                        render();
                        break;
                    }
                }
#else
                break;
#endif
                sleepMs(50);
            }
            if (!m_running) break;
        }

#ifdef _WIN32
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 0 || ch == 0xE0) {
                int code = _getch();
                handleSpecialKey(code);
            } else {
                handleKey(ch);
            }
            render();
        }
#endif
        sleepMs(16);
    }

    restoreConsole();
}

void Engine::runHeadless(int maxTicks, unsigned int seed) {
    init(seed);

    for (int tick = 0; tick < maxTicks; ++tick) {
        if (m_gameOver || m_victory) {
            init(seed + static_cast<unsigned int>(tick) + 1);
        }

        // Check if any weapon in inventory can be equipped
        for (size_t i = 0; i < m_inventory.size(); ++i) {
            if (m_inventory[i].category == ItemCategory::Weapon && !m_inventory[i].equipped) {
                useOrEquipItem(i);
                break;
            }
        }
        // Check if any armor in inventory can be equipped
        for (size_t i = 0; i < m_inventory.size(); ++i) {
            if (m_inventory[i].category == ItemCategory::Armor && !m_inventory[i].equipped) {
                useOrEquipItem(i);
                break;
            }
        }
        // If HP is low (< 50%) and we have a potion, drink it!
        if (m_player.hp < m_player.maxHp / 2) {
            for (size_t i = 0; i < m_inventory.size(); ++i) {
                if (m_inventory[i].category == ItemCategory::Potion) {
                    useOrEquipItem(i);
                    break;
                }
            }
        }

        // Find stairs position on current floor
        Point stairs{-1, -1};
        for (int y = 0; y < m_map.getHeight(); ++y) {
            for (int x = 0; x < m_map.getWidth(); ++x) {
                if (m_map.getTile(x, y) == TileType::StairsDown) {
                    stairs = {x, y};
                    break;
                }
            }
            if (stairs.x != -1) break;
        }

        // 1. If standing on stairs, descend!
        if (m_player.pos == stairs) {
            descendStairs();
            continue;
        }

        // 2. Check if any monster is adjacent -> attack it
        bool acted = false;
        const int dx[4] = {0, 0, 1, -1};
        const int dy[4] = {1, -1, 0, 0};
        for (int i = 0; i < 4; ++i) {
            Point adj{m_player.pos.x + dx[i], m_player.pos.y + dy[i]};
            for (auto& m : m_monsters) {
                if (m.isAlive() && m.pos == adj) {
                    stepPlayer(dx[i], dy[i]);
                    acted = true;
                    break;
                }
            }
            if (acted) break;
        }
        if (acted) continue;

        // 2b. Check if any visible monster in range [2, 6] with clear LOS -> shoot it
        for (const auto& m : m_monsters) {
            if (m.isAlive() && m_fow.isVisible(m.pos.x, m.pos.y)) {
                int d = std::max(std::abs(m.pos.x - m_player.pos.x), std::abs(m.pos.y - m_player.pos.y));
                if (d >= 2 && d <= 6 && LineOfSight::hasLineOfSight(m_map, m_player.pos, m.pos)) {
                    shootAt(m.pos);
                    acted = true;
                    break;
                }
            }
        }
        if (acted) continue;

        // 3. If there is an item nearby on the floor, step towards it to pick it up
        if (m_inventory.size() < MAX_INVENTORY_CAPACITY) {
            for (const auto& item : m_items) {
                if (!item.pickedUp) {
                    auto path = Pathfinding::findPath(m_map, m_player.pos, item.pos);
                    if (!path.empty()) {
                        Point step = path[0];
                        if (stepPlayer(step.x - m_player.pos.x, step.y - m_player.pos.y)) {
                            acted = true;
                            break;
                        }
                    }
                }
            }
            if (acted) continue;
        }

        // 4. Pathfind towards stairs
        if (stairs.x != -1) {
            auto path = Pathfinding::findPath(m_map, m_player.pos, stairs);
            if (!path.empty()) {
                Point step = path[0];
                if (stepPlayer(step.x - m_player.pos.x, step.y - m_player.pos.y)) {
                    acted = true;
                }
            }
        }

        // 5. If pathfinding to stairs failed, explore towards any walkable adjacent tile
        if (!acted) {
            for (int i = 0; i < 4; ++i) {
                Point next{m_player.pos.x + dx[i], m_player.pos.y + dy[i]};
                if (m_map.isInBounds(next.x, next.y) &&
                    (m_map.isWalkable(next.x, next.y) || m_map.getTile(next.x, next.y) == TileType::DoorClosed)) {
                    if (stepPlayer(dx[i], dy[i])) {
                        acted = true;
                        break;
                    }
                }
            }
        }

        // 6. If completely blocked, wait turn (triggers safe rest if no enemies nearby)
        if (!acted) {
            waitTurn();
        }
    }
}

// =========================================================================
// Native Win32 GDI 2D Tile GUI Implementation
// =========================================================================

#ifdef _WIN32

static LRESULT CALLBACK GuiWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Engine* engine = reinterpret_cast<Engine*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        case WM_ERASEBKGND:
            return 1; // Double buffering handled in WM_PAINT

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (engine) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;

                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
                HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

                engine->renderGui(memDC, w, h);

                BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

                SelectObject(memDC, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDC);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (engine && engine->isTargetingMode()) {
                int tx = LOWORD(lParam) / 14;
                int ty = HIWORD(lParam) / 14;
                if (engine->getMap().isInBounds(tx, ty)) {
                    engine->setTargetCursor({tx, ty});
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            if (engine && engine->isInventoryOpen()) {
                int mouseX = LOWORD(lParam);
                int mouseY = HIWORD(lParam);
                engine->handleInventoryClick(mouseX, mouseY, false);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (engine && engine->isTargetingMode()) {
                int tx = LOWORD(lParam) / 14;
                int ty = HIWORD(lParam) / 14;
                if (engine->getMap().isInBounds(tx, ty)) {
                    engine->setTargetCursor({tx, ty});
                    engine->shootAtTarget();
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            break;
        }

        case WM_RBUTTONDOWN: {
            if (engine && engine->isInventoryOpen()) {
                int mouseX = LOWORD(lParam);
                int mouseY = HIWORD(lParam);
                engine->handleInventoryClick(mouseX, mouseY, true);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (engine && engine->isTargetingMode()) {
                engine->setTargetingMode(false);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }

        case WM_KEYDOWN: {
            if (!engine) break;
            bool handled = false;

            if (engine->isInventoryOpen()) {
                if (wParam == VK_ESCAPE || wParam == 'I') {
                    engine->setInventoryOpen(false);
                    handled = true;
                } else if (wParam == VK_UP || wParam == 'W') {
                    engine->selectPrevInventoryItem();
                    handled = true;
                } else if (wParam == VK_DOWN || wParam == 'S') {
                    engine->selectNextInventoryItem();
                    handled = true;
                } else if (wParam == VK_RETURN || wParam == VK_SPACE) {
                    engine->useOrEquipSelectedItem();
                    handled = true;
                } else if (wParam == 'D' || wParam == VK_DELETE) {
                    engine->dropSelectedItem();
                    handled = true;
                } else if (wParam >= '1' && wParam <= '9') {
                    size_t idx = static_cast<size_t>(wParam - '1');
                    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    engine->setInventorySelectedIndex(idx);
                    if (shift) {
                        engine->dropItem(idx);
                    } else {
                        engine->useOrEquipItem(idx);
                    }
                    handled = true;
                } else if (wParam == '0') {
                    size_t idx = 9;
                    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    engine->setInventorySelectedIndex(idx);
                    if (shift) {
                        engine->dropItem(idx);
                    } else {
                        engine->useOrEquipItem(idx);
                    }
                    handled = true;
                } else if (wParam >= VK_NUMPAD1 && wParam <= VK_NUMPAD9) {
                    size_t idx = static_cast<size_t>(wParam - VK_NUMPAD1);
                    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    engine->setInventorySelectedIndex(idx);
                    if (shift) {
                        engine->dropItem(idx);
                    } else {
                        engine->useOrEquipItem(idx);
                    }
                    handled = true;
                } else if (wParam == VK_NUMPAD0) {
                    size_t idx = 9;
                    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    engine->setInventorySelectedIndex(idx);
                    if (shift) {
                        engine->dropItem(idx);
                    } else {
                        engine->useOrEquipItem(idx);
                    }
                    handled = true;
                } else if (wParam >= 'A' && wParam <= 'J') {
                    size_t idx = static_cast<size_t>(wParam - 'A');
                    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    engine->setInventorySelectedIndex(idx);
                    if (shift) {
                        engine->dropItem(idx);
                    } else {
                        engine->useOrEquipItem(idx);
                    }
                    handled = true;
                }
            } else if (engine->isTargetingMode()) {
                if (wParam == VK_ESCAPE || wParam == 'Q') {
                    engine->setTargetingMode(false);
                    handled = true;
                } else if (wParam == 'F' || wParam == VK_RETURN || wParam == VK_SPACE) {
                    engine->shootAtTarget();
                    handled = true;
                } else if (wParam == 'W' || wParam == VK_UP) {
                    engine->moveTargetCursor(0, -1);
                    handled = true;
                } else if (wParam == 'S' || wParam == VK_DOWN) {
                    engine->moveTargetCursor(0, 1);
                    handled = true;
                } else if (wParam == 'A' || wParam == VK_LEFT) {
                    engine->moveTargetCursor(-1, 0);
                    handled = true;
                } else if (wParam == 'D' || wParam == VK_RIGHT) {
                    engine->moveTargetCursor(1, 0);
                    handled = true;
                }
            } else {
                switch (wParam) {
                    case VK_ESCAPE:
                    case 'Q':
                        DestroyWindow(hwnd);
                        handled = true;
                        break;
                    case 'R':
                        engine->restart();
                        handled = true;
                        break;
                    case 'I':
                        engine->setInventoryOpen(true);
                        handled = true;
                        break;
                    case 'F':
                        engine->toggleTargetingMode();
                        handled = true;
                        break;
                    case 'W':
                    case VK_UP:
                        engine->stepPlayer(0, -1);
                        handled = true;
                        break;
                    case 'S':
                    case VK_DOWN:
                        engine->stepPlayer(0, 1);
                        handled = true;
                        break;
                    case 'A':
                    case VK_LEFT:
                        engine->stepPlayer(-1, 0);
                        handled = true;
                        break;
                    case 'D':
                    case VK_RIGHT:
                        engine->stepPlayer(1, 0);
                        handled = true;
                        break;
                    case VK_SPACE:
                        engine->waitTurn();
                        handled = true;
                        break;
                    case VK_OEM_PERIOD:
                        if (GetKeyState(VK_SHIFT) & 0x8000) {
                            engine->descendStairs();
                        } else {
                            engine->waitTurn();
                        }
                        handled = true;
                        break;
                    default:
                        break;
                }
            }

            if (handled) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_CHAR: {
            if (!engine) break;
            bool handled = false;

            if (engine->isInventoryOpen()) {
                if (wParam == 'i' || wParam == 'I' || wParam == 27) {
                    engine->setInventoryOpen(false);
                    handled = true;
                } else if (wParam == '\r' || wParam == ' ') {
                    engine->useOrEquipSelectedItem();
                    handled = true;
                } else if (wParam == 'd' || wParam == 'D') {
                    engine->dropSelectedItem();
                    handled = true;
                } else if (wParam == 'w' || wParam == 'W') {
                    engine->selectPrevInventoryItem();
                    handled = true;
                } else if (wParam == 's' || wParam == 'S') {
                    engine->selectNextInventoryItem();
                    handled = true;
                } else if (wParam >= '1' && wParam <= '9') {
                    size_t idx = static_cast<size_t>(wParam - '1');
                    engine->setInventorySelectedIndex(idx);
                    engine->useOrEquipItem(idx);
                    handled = true;
                } else if (wParam == '0') {
                    size_t idx = 9;
                    engine->setInventorySelectedIndex(idx);
                    engine->useOrEquipItem(idx);
                    handled = true;
                } else if (wParam >= 'a' && wParam <= 'j') {
                    size_t idx = static_cast<size_t>(wParam - 'a');
                    engine->setInventorySelectedIndex(idx);
                    engine->useOrEquipItem(idx);
                    handled = true;
                } else if (wParam >= 'A' && wParam <= 'J') {
                    size_t idx = static_cast<size_t>(wParam - 'A');
                    engine->setInventorySelectedIndex(idx);
                    engine->useOrEquipItem(idx);
                    handled = true;
                } else if (wParam == '!') {
                    engine->dropItem(0); handled = true;
                } else if (wParam == '@') {
                    engine->dropItem(1); handled = true;
                } else if (wParam == '#') {
                    engine->dropItem(2); handled = true;
                } else if (wParam == '$') {
                    engine->dropItem(3); handled = true;
                } else if (wParam == '%') {
                    engine->dropItem(4); handled = true;
                } else if (wParam == '^') {
                    engine->dropItem(5); handled = true;
                } else if (wParam == '&') {
                    engine->dropItem(6); handled = true;
                } else if (wParam == '*') {
                    engine->dropItem(7); handled = true;
                } else if (wParam == '(') {
                    engine->dropItem(8); handled = true;
                } else if (wParam == ')') {
                    engine->dropItem(9); handled = true;
                }
            } else if (engine->isTargetingMode()) {
                if (wParam == 27 || wParam == 'q' || wParam == 'Q') {
                    engine->setTargetingMode(false);
                    handled = true;
                } else if (wParam == 'f' || wParam == 'F' || wParam == '\r' || wParam == ' ') {
                    engine->shootAtTarget();
                    handled = true;
                } else if (wParam == 'w' || wParam == 'W') {
                    engine->moveTargetCursor(0, -1);
                    handled = true;
                } else if (wParam == 's' || wParam == 'S') {
                    engine->moveTargetCursor(0, 1);
                    handled = true;
                } else if (wParam == 'a' || wParam == 'A') {
                    engine->moveTargetCursor(-1, 0);
                    handled = true;
                } else if (wParam == 'd' || wParam == 'D') {
                    engine->moveTargetCursor(1, 0);
                    handled = true;
                }
            } else {
                if (wParam == '>') {
                    engine->descendStairs();
                    handled = true;
                } else if (wParam == '.') {
                    engine->waitTurn();
                    handled = true;
                } else if (wParam == 'r' || wParam == 'R') {
                    engine->restart();
                    handled = true;
                } else if (wParam == 'i' || wParam == 'I') {
                    engine->setInventoryOpen(true);
                    handled = true;
                } else if (wParam == 'f' || wParam == 'F') {
                    engine->toggleTargetingMode();
                    handled = true;
                } else if (wParam == 'q' || wParam == 'Q') {
                    DestroyWindow(hwnd);
                    handled = true;
                }
            }

            if (handled) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#endif // _WIN32

void Engine::runGui() {
#ifdef _WIN32
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = GuiWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = "RoguelikeEngineGuiClass";

    RegisterClassEx(&wc);

    const int TILE_SIZE = 14;
    const int CLIENT_WIDTH = 80 * TILE_SIZE; // 1120
    const int CLIENT_HEIGHT = 25 * TILE_SIZE + 170; // 520

    RECT rc = { 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int winWidth = rc.right - rc.left;
    int winHeight = rc.bottom - rc.top;

    HWND hwnd = CreateWindowEx(
        0,
        "RoguelikeEngineGuiClass",
        "Roguelike Dungeon - 2D Native Win32 GUI",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winWidth, winHeight,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    if (!hwnd) {
        std::cerr << "Failed to create Win32 window, falling back to CLI.\n";
        runInteractive();
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#else
    runInteractive();
#endif
}

void Engine::renderGui(void* hdcVoid, int width, int height) {
#ifdef _WIN32
    HDC memDC = static_cast<HDC>(hdcVoid);

    // 1. Clear whole window
    HBRUSH bgBrush = CreateSolidBrush(RGB(10, 10, 14));
    RECT rcAll = { 0, 0, width, height };
    FillRect(memDC, &rcAll, bgBrush);
    DeleteObject(bgBrush);

    // Fonts
    HFONT hFontRegular = CreateFontA(
        14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas"
    );
    HFONT hFontTile = CreateFontA(
        11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas"
    );
    HFONT hFontBanner = CreateFontA(
        26, 0, 0, 0, FW_HEAVY, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas"
    );

    // Tile pens and brushes
    HBRUSH hbrFloorVis = CreateSolidBrush(RGB(50, 50, 55));
    HBRUSH hbrFloorDim = CreateSolidBrush(RGB(22, 22, 25));

    HBRUSH hbrWallCenterVis = CreateSolidBrush(RGB(75, 80, 90));
    HBRUSH hbrWallCenterDim = CreateSolidBrush(RGB(32, 34, 38));
    HPEN hpenLightBevelVis = CreatePen(PS_SOLID, 1, RGB(130, 138, 150));
    HPEN hpenLightBevelDim = CreatePen(PS_SOLID, 1, RGB(55, 58, 65));
    HPEN hpenDarkBevelVis = CreatePen(PS_SOLID, 1, RGB(35, 38, 45));
    HPEN hpenDarkBevelDim = CreatePen(PS_SOLID, 1, RGB(16, 17, 20));

    HBRUSH hbrDoorVis = CreateSolidBrush(RGB(160, 90, 44));
    HBRUSH hbrDoorDim = CreateSolidBrush(RGB(70, 40, 20));
    HPEN hpenDoorFrame = CreatePen(PS_SOLID, 1, RGB(90, 45, 18));

    HPEN hpenStairsVis = CreatePen(PS_SOLID, 2, RGB(40, 215, 215));
    HPEN hpenStairsDim = CreatePen(PS_SOLID, 2, RGB(18, 90, 90));

    HBRUSH hbrGreen = CreateSolidBrush(RGB(40, 220, 70));
    HBRUSH hbrYellow = CreateSolidBrush(RGB(230, 200, 40));
    HBRUSH hbrRed = CreateSolidBrush(RGB(220, 40, 40));
    HBRUSH hbrDarkRed = CreateSolidBrush(RGB(70, 15, 15));
    HBRUSH hbrExpBg = CreateSolidBrush(RGB(35, 35, 45));
    HBRUSH hbrExpFg = CreateSolidBrush(RGB(170, 70, 245));

    HPEN hpenNull = static_cast<HPEN>(GetStockObject(NULL_PEN));

    const int TILE_SIZE = 14;
    int mapW = m_map.getWidth();
    int mapH = m_map.getHeight();

    // 2. Render Map Tiles
    for (int y = 0; y < mapH; ++y) {
        for (int x = 0; x < mapW; ++x) {
            bool visible = m_fow.isVisible(x, y);
            bool explored = m_fow.isExplored(x, y);

            if (!explored) {
                continue; // Pitch black
            }

            int px = x * TILE_SIZE;
            int py = y * TILE_SIZE;
            RECT rTile = { px, py, px + TILE_SIZE, py + TILE_SIZE };
            TileType t = m_map.getTile(x, y);

            if (t == TileType::Wall) {
                // 3D Beveled Stone Wall
                FillRect(memDC, &rTile, visible ? hbrWallCenterVis : hbrWallCenterDim);

                HPEN hpenLight = visible ? hpenLightBevelVis : hpenLightBevelDim;
                HPEN hpenDark = visible ? hpenDarkBevelVis : hpenDarkBevelDim;

                HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, hpenLight));
                MoveToEx(memDC, px, py, NULL);
                LineTo(memDC, px + TILE_SIZE, py);
                MoveToEx(memDC, px, py, NULL);
                LineTo(memDC, px, py + TILE_SIZE);

                SelectObject(memDC, hpenDark);
                MoveToEx(memDC, px, py + TILE_SIZE - 1, NULL);
                LineTo(memDC, px + TILE_SIZE, py + TILE_SIZE - 1);
                MoveToEx(memDC, px + TILE_SIZE - 1, py, NULL);
                LineTo(memDC, px + TILE_SIZE - 1, py + TILE_SIZE);

                SelectObject(memDC, oldPen);
            } else if (t == TileType::Floor) {
                FillRect(memDC, &rTile, visible ? hbrFloorVis : hbrFloorDim);
                SetPixel(memDC, px + TILE_SIZE / 2, py + TILE_SIZE / 2,
                         visible ? RGB(70, 70, 75) : RGB(30, 30, 35));
            } else if (t == TileType::DoorClosed) {
                FillRect(memDC, &rTile, visible ? hbrDoorVis : hbrDoorDim);
                HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, hpenDoorFrame));
                MoveToEx(memDC, px, py, NULL);
                LineTo(memDC, px, py + TILE_SIZE);
                MoveToEx(memDC, px + TILE_SIZE - 1, py, NULL);
                LineTo(memDC, px + TILE_SIZE - 1, py + TILE_SIZE);
                SelectObject(memDC, oldPen);
                if (visible) {
                    SetPixel(memDC, px + TILE_SIZE / 2 + 1, py + TILE_SIZE / 2, RGB(220, 180, 50));
                }
            } else if (t == TileType::DoorOpen) {
                FillRect(memDC, &rTile, visible ? hbrFloorVis : hbrFloorDim);
                HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, visible ? hpenLightBevelVis : hpenLightBevelDim));
                MoveToEx(memDC, px + 2, py + 2, NULL);
                LineTo(memDC, px + TILE_SIZE - 2, py + 4);
                SelectObject(memDC, oldPen);
            } else if (t == TileType::StairsDown) {
                FillRect(memDC, &rTile, visible ? hbrFloorVis : hbrFloorDim);
                HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, visible ? hpenStairsVis : hpenStairsDim));
                MoveToEx(memDC, px + 2, py + 3, NULL);
                LineTo(memDC, px + TILE_SIZE - 2, py + 3);
                MoveToEx(memDC, px + 4, py + 7, NULL);
                LineTo(memDC, px + TILE_SIZE - 4, py + 7);
                MoveToEx(memDC, px + 6, py + 11, NULL);
                LineTo(memDC, px + TILE_SIZE - 6, py + 11);
                SelectObject(memDC, oldPen);
            }
        }
    }

    // 3. Render Floor Items
    HBRUSH hbrPotion = CreateSolidBrush(RGB(220, 40, 130));
    HBRUSH hbrWeapon = CreateSolidBrush(RGB(80, 200, 240));
    HBRUSH hbrArmor = CreateSolidBrush(RGB(170, 185, 210));
    HBRUSH hbrScroll = CreateSolidBrush(RGB(240, 220, 120));
    HBRUSH hbrGold = CreateSolidBrush(RGB(220, 175, 50));

    SetBkMode(memDC, TRANSPARENT);
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, hFontTile));

    for (const auto& item : m_items) {
        if (!item.pickedUp && m_fow.isVisible(item.pos.x, item.pos.y)) {
            int ix = item.pos.x * TILE_SIZE;
            int iy = item.pos.y * TILE_SIZE;

            HBRUSH itemBrush = hbrPotion;
            COLORREF itemTxtColor = RGB(255, 255, 255);
            if (item.category == ItemCategory::Weapon) {
                itemBrush = hbrWeapon;
                itemTxtColor = RGB(10, 40, 60);
            } else if (item.category == ItemCategory::Armor) {
                itemBrush = hbrArmor;
                itemTxtColor = RGB(20, 30, 40);
            } else if (item.category == ItemCategory::Scroll) {
                itemBrush = hbrScroll;
                itemTxtColor = RGB(50, 40, 10);
            }

            HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, hpenNull));
            HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC, itemBrush));
            Ellipse(memDC, ix + 2, iy + 2, ix + TILE_SIZE - 2, iy + TILE_SIZE - 2);

            SetTextColor(memDC, itemTxtColor);
            char sym[2] = { item.symbol, '\0' };
            RECT rSym = { ix, iy, ix + TILE_SIZE, iy + TILE_SIZE };
            DrawTextA(memDC, sym, 1, &rSym, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(memDC, oldBrush);
            SelectObject(memDC, oldPen);
        }
    }

    // 4. Render Entities
    HBRUSH hbrGoblin = CreateSolidBrush(RGB(40, 180, 80));
    HBRUSH hbrSkeleton = CreateSolidBrush(RGB(230, 230, 220));
    HBRUSH hbrOrc = CreateSolidBrush(RGB(190, 40, 40));
    HBRUSH hbrDragon = CreateSolidBrush(RGB(245, 30, 30));
    HPEN hpenDarkEntity = CreatePen(PS_SOLID, 1, RGB(15, 15, 20));

    // Monsters
    for (const auto& m : m_monsters) {
        if (m.isAlive() && m_fow.isVisible(m.pos.x, m.pos.y)) {
            int mx = m.pos.x * TILE_SIZE;
            int my = m.pos.y * TILE_SIZE;

            HBRUSH hbrMonster = hbrGoblin;
            COLORREF txtColor = RGB(255, 255, 255);
            if (m.name == "Skeleton" || m.name == "Skeleton Archer") {
                hbrMonster = hbrSkeleton;
                txtColor = RGB(30, 30, 30);
            } else if (m.name == "Orc" || m.name == "Orc Warrior") {
                hbrMonster = hbrOrc;
            } else if (m.name == "Dragon") {
                hbrMonster = hbrDragon;
            }

            HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, hpenDarkEntity));
            HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC, hbrMonster));
            Ellipse(memDC, mx + 1, my + 1, mx + TILE_SIZE - 1, my + TILE_SIZE - 1);
            SelectObject(memDC, oldBrush);
            SelectObject(memDC, oldPen);

            SetTextColor(memDC, txtColor);
            char sym[2] = { m.symbol, '\0' };
            RECT rSym = { mx, my, mx + TILE_SIZE, my + TILE_SIZE };
            DrawTextA(memDC, sym, 1, &rSym, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Mini HP Bar above head if damaged
            if (m.hp < m.maxHp) {
                int barW = TILE_SIZE;
                int barH = 2;
                int barX = mx;
                int barY = my - 3;
                RECT rBg = { barX, barY, barX + barW, barY + barH };
                FillRect(memDC, &rBg, hbrDarkRed);
                int curW = std::max(1, (m.hp * barW) / m.maxHp);
                RECT rFg = { barX, barY, barX + curW, barY + barH };
                FillRect(memDC, &rFg, hbrGreen);
            }
        }
    }

    // Player Avatar
    int px = m_player.pos.x * TILE_SIZE;
    int py = m_player.pos.y * TILE_SIZE;

    HBRUSH hbrPlayer = CreateSolidBrush(RGB(30, 120, 255)); // Royal blue
    HPEN hpenPlayerGlow = CreatePen(PS_SOLID, 2, RGB(255, 215, 0)); // Golden glowing border

    HPEN oldPenP = static_cast<HPEN>(SelectObject(memDC, hpenPlayerGlow));
    HBRUSH oldBrushP = static_cast<HBRUSH>(SelectObject(memDC, hbrPlayer));
    Ellipse(memDC, px + 1, py + 1, px + TILE_SIZE - 1, py + TILE_SIZE - 1);
    SelectObject(memDC, oldBrushP);
    SelectObject(memDC, oldPenP);

    SetTextColor(memDC, RGB(255, 240, 100));
    RECT rPlayerSym = { px, py, px + TILE_SIZE, py + TILE_SIZE };
    DrawTextA(memDC, "@", 1, &rPlayerSym, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Mini HP Bar directly above player
    {
        int barW = TILE_SIZE;
        int barH = 3;
        int barX = px;
        int barY = py - 4;
        RECT rBg = { barX, barY, barX + barW, barY + barH };
        FillRect(memDC, &rBg, hbrDarkRed);
        int curW = std::max(0, (m_player.hp * barW) / m_player.maxHp);
        RECT rFg = { barX, barY, barX + curW, barY + barH };
        HBRUSH curHpBr = (m_player.hp > m_player.maxHp / 2) ? hbrGreen :
                         ((m_player.hp > m_player.maxHp / 4) ? hbrYellow : hbrRed);
        FillRect(memDC, &rFg, curHpBr);
    }

    // 4b. Render Targeting Cursor & Line of Sight
    if (m_targetingMode) {
        int tx = m_targetCursor.x * TILE_SIZE;
        int ty = m_targetCursor.y * TILE_SIZE;

        int dist = std::max(std::abs(m_targetCursor.x - m_player.pos.x), std::abs(m_targetCursor.y - m_player.pos.y));
        bool hasLOS = LineOfSight::hasLineOfSight(m_map, m_player.pos, m_targetCursor);
        bool isValid = (dist <= 6 && hasLOS);

        COLORREF targetColor = isValid ? RGB(0, 240, 255) : RGB(255, 60, 60);

        // Draw line from player center to cursor center
        HPEN hpenLine = CreatePen(PS_DOT, 1, targetColor);
        HPEN oldP = static_cast<HPEN>(SelectObject(memDC, hpenLine));
        SetBkMode(memDC, TRANSPARENT);
        MoveToEx(memDC, px + TILE_SIZE / 2, py + TILE_SIZE / 2, NULL);
        LineTo(memDC, tx + TILE_SIZE / 2, ty + TILE_SIZE / 2);
        SelectObject(memDC, oldP);
        DeleteObject(hpenLine);

        // Draw Crosshair / Reticle on target cursor
        HPEN hpenReticle = CreatePen(PS_SOLID, 2, targetColor);
        oldP = static_cast<HPEN>(SelectObject(memDC, hpenReticle));

        // Corner brackets around the target tile
        // Top-Left corner
        MoveToEx(memDC, tx, ty, NULL);
        LineTo(memDC, tx + 5, ty);
        MoveToEx(memDC, tx, ty, NULL);
        LineTo(memDC, tx, ty + 5);

        // Top-Right corner
        MoveToEx(memDC, tx + TILE_SIZE, ty, NULL);
        LineTo(memDC, tx + TILE_SIZE - 5, ty);
        MoveToEx(memDC, tx + TILE_SIZE, ty, NULL);
        LineTo(memDC, tx + TILE_SIZE, ty + 5);

        // Bottom-Left corner
        MoveToEx(memDC, tx, ty + TILE_SIZE, NULL);
        LineTo(memDC, tx + 5, ty + TILE_SIZE);
        MoveToEx(memDC, tx, ty + TILE_SIZE, NULL);
        LineTo(memDC, tx, ty + TILE_SIZE - 5);

        // Bottom-Right corner
        MoveToEx(memDC, tx + TILE_SIZE, ty + TILE_SIZE, NULL);
        LineTo(memDC, tx + TILE_SIZE - 5, ty + TILE_SIZE);
        MoveToEx(memDC, tx + TILE_SIZE, ty + TILE_SIZE, NULL);
        LineTo(memDC, tx + TILE_SIZE, ty + TILE_SIZE - 5);

        // Center dot
        SetPixel(memDC, tx + TILE_SIZE / 2, ty + TILE_SIZE / 2, targetColor);

        SelectObject(memDC, oldP);
        DeleteObject(hpenReticle);
    }

    // 5. Sleek Bottom HUD
    int hudY = mapH * TILE_SIZE;
    HBRUSH hbrHudBg = CreateSolidBrush(RGB(20, 22, 28));
    RECT rHud = { 0, hudY, width, height };
    FillRect(memDC, &rHud, hbrHudBg);

    // Accent line divider
    HPEN hpenAccent = CreatePen(PS_SOLID, 2, RGB(45, 110, 210));
    HPEN oldPenHud = static_cast<HPEN>(SelectObject(memDC, hpenAccent));
    MoveToEx(memDC, 0, hudY, NULL);
    LineTo(memDC, width, hudY);

    MoveToEx(memDC, 540, hudY + 8, NULL);
    LineTo(memDC, 540, height - 8);
    SelectObject(memDC, oldPenHud);

    SelectObject(memDC, hFontRegular);

    // Left Column: Stats
    SetTextColor(memDC, RGB(100, 220, 255));
    std::string s1 = "Floor: " + std::to_string(m_floor) +
                     "   |   Turn: " + std::to_string(m_turnCount) +
                     "   |   Level: " + std::to_string(m_playerLevel);
    TextOutA(memDC, 20, hudY + 12, s1.c_str(), static_cast<int>(s1.size()));

    // EXP Bar
    SetTextColor(memDC, RGB(200, 160, 255));
    std::string sExp = "EXP: " + std::to_string(m_playerExp) + " / " + std::to_string(m_playerExpToNext);
    TextOutA(memDC, 20, hudY + 36, sExp.c_str(), static_cast<int>(sExp.size()));

    int expBarX = 180;
    int expBarY = hudY + 38;
    int expBarW = 140;
    int expBarH = 10;
    RECT rExpBg = { expBarX, expBarY, expBarX + expBarW, expBarY + expBarH };
    FillRect(memDC, &rExpBg, hbrExpBg);
    int expFilled = (m_playerExp * expBarW) / std::max(1, m_playerExpToNext);
    expFilled = std::max(0, std::min(expBarW, expFilled));
    RECT rExpFg = { expBarX, expBarY, expBarX + expFilled, expBarY + expBarH };
    FillRect(memDC, &rExpFg, hbrExpFg);

    // HP Bar
    SetTextColor(memDC, RGB(255, 120, 120));
    std::string sHp = "HP:  " + std::to_string(m_player.hp) + " / " + std::to_string(m_player.maxHp);
    TextOutA(memDC, 20, hudY + 60, sHp.c_str(), static_cast<int>(sHp.size()));

    int hpBarX = 180;
    int hpBarY = hudY + 62;
    int hpBarW = 180;
    int hpBarH = 12;
    RECT rHpBg = { hpBarX, hpBarY, hpBarX + hpBarW, hpBarY + hpBarH };
    FillRect(memDC, &rHpBg, hbrDarkRed);
    int hpFilled = (m_player.hp * hpBarW) / std::max(1, m_player.maxHp);
    hpFilled = std::max(0, std::min(hpBarW, hpFilled));
    RECT rHpFg = { hpBarX, hpBarY, hpBarX + hpFilled, hpBarY + hpBarH };
    HBRUSH curHpBr = (m_player.hp > m_player.maxHp / 2) ? hbrGreen :
                     ((m_player.hp > m_player.maxHp / 4) ? hbrYellow : hbrRed);
    FillRect(memDC, &rHpFg, curHpBr);

    // Attack & Defense with equipped gear names
    const Item* wep = getEquippedWeapon();
    const Item* arm = getEquippedArmor();
    std::string wepStr = wep ? (" (+" + std::to_string(wep->atkBonus) + " " + wep->name + ")") : "";
    std::string armStr = arm ? (" (+" + std::to_string(arm->defBonus) + " " + arm->name + ")") : "";

    SetTextColor(memDC, RGB(220, 180, 100));
    std::string sCombatStats = "Attack: " + std::to_string(getTotalPlayerAttack()) + wepStr +
                               "   |   Defense: " + std::to_string(getTotalPlayerDefense()) + armStr;
    TextOutA(memDC, 20, hudY + 86, sCombatStats.c_str(), static_cast<int>(sCombatStats.size()));

    // Controls Hints
    SetTextColor(memDC, RGB(140, 150, 165));
    std::string c1 = "[WASD / Arrows] Move & Attack   |   [F] Shoot   |   [I] Inventory (" +
                     std::to_string(m_inventory.size()) + "/10)   |   [Space / .] Rest";
    TextOutA(memDC, 20, hudY + 112, c1.c_str(), static_cast<int>(c1.size()));

    if (m_targetingMode) {
        SetTextColor(memDC, RGB(0, 240, 255));
        std::string tModeStr = "*** TARGETING MODE *** [WASD/Click] Aim | [Enter/Space/F/L-Click] Fire | [ESC/R-Click] Cancel";
        TextOutA(memDC, 20, hudY + 134, tModeStr.c_str(), static_cast<int>(tModeStr.size()));
    } else {
        std::string c2 = "[>] Descend Stairs   |   [R] Quick Restart   |   [Q / ESC] Quit";
        TextOutA(memDC, 20, hudY + 134, c2.c_str(), static_cast<int>(c2.size()));
    }

    // Right Column: Combat Log
    SetTextColor(memDC, RGB(240, 200, 60));
    std::string logHeader = "--- Recent Combat Events ---";
    TextOutA(memDC, 560, hudY + 12, logHeader.c_str(), static_cast<int>(logHeader.size()));

    int msgCount = static_cast<int>(m_messageLog.size());
    int startIdx = std::max(0, msgCount - 5);
    int logY = hudY + 36;
    for (int i = startIdx; i < msgCount; ++i) {
        if (i == msgCount - 1) {
            SetTextColor(memDC, RGB(255, 255, 255));
        } else {
            SetTextColor(memDC, RGB(165, 170, 180));
        }
        std::string logLine = "> " + m_messageLog[static_cast<size_t>(i)];
        TextOutA(memDC, 560, logY, logLine.c_str(), static_cast<int>(logLine.size()));
        logY += 22;
    }

    // 6. Inventory Modal (if open)
    if (m_inventoryOpen) {
        int modalW = 560;
        int modalH = 330;
        int modalX = (width - modalW) / 2;
        int modalY = 20;

        HBRUSH hbrModalBg = CreateSolidBrush(RGB(16, 18, 24));
        HPEN hpenModalBorder = CreatePen(PS_SOLID, 2, RGB(240, 200, 60)); // Gold border
        HBRUSH hbrSelectedRow = CreateSolidBrush(RGB(30, 48, 80));         // Row highlight background
        HPEN hpenSelectedBorder = CreatePen(PS_SOLID, 1, RGB(255, 215, 0)); // Highlight border

        HPEN oldP = static_cast<HPEN>(SelectObject(memDC, hpenModalBorder));
        HBRUSH oldB = static_cast<HBRUSH>(SelectObject(memDC, hbrModalBg));
        Rectangle(memDC, modalX, modalY, modalX + modalW, modalY + modalH);
        SelectObject(memDC, oldB);
        SelectObject(memDC, oldP);

        // Modal Title
        SetTextColor(memDC, RGB(255, 215, 0));
        std::string title = "=== INVENTORY & EQUIPMENT (" + std::to_string(m_inventory.size()) + "/10) ===";
        RECT rTitle = { modalX, modalY + 12, modalX + modalW, modalY + 35 };
        DrawTextA(memDC, title.c_str(), static_cast<int>(title.size()), &rTitle, DT_CENTER | DT_SINGLELINE);

        // Ensure selection is within bounds
        if (m_inventorySelectedIndex >= m_inventory.size() && !m_inventory.empty()) {
            const_cast<Engine*>(this)->m_inventorySelectedIndex = m_inventory.size() - 1;
        }

        // Items list
        int itemY = modalY + 45;
        if (m_inventory.empty()) {
            SetTextColor(memDC, RGB(140, 140, 150));
            std::string emptyMsg = "(Backpack is empty. Explore dungeon tiles to find items!)";
            RECT rEmpty = { modalX, itemY + 40, modalX + modalW, itemY + 70 };
            DrawTextA(memDC, emptyMsg.c_str(), static_cast<int>(emptyMsg.size()), &rEmpty, DT_CENTER | DT_SINGLELINE);
        } else {
            for (size_t i = 0; i < m_inventory.size(); ++i) {
                const auto& it = m_inventory[i];
                bool isSelected = (i == m_inventorySelectedIndex);

                if (isSelected) {
                    RECT rSel = { modalX + 15, itemY - 1, modalX + modalW - 15, itemY + 23 };
                    FillRect(memDC, &rSel, hbrSelectedRow);
                    FrameRect(memDC, &rSel, hbrGold);
                }

                std::string prefix = isSelected ? " > " : "   ";
                std::string itemLine = prefix + "[" + std::to_string(i + 1) + "] " + it.name;

                if (it.category == ItemCategory::Weapon) {
                    itemLine += "  (Atk +" + std::to_string(it.atkBonus) + ")";
                } else if (it.category == ItemCategory::Armor) {
                    itemLine += "  (Def +" + std::to_string(it.defBonus) + ")";
                } else if (it.category == ItemCategory::Potion) {
                    itemLine += "  (Heals +" + std::to_string(it.healAmount) + " HP)";
                } else if (it.category == ItemCategory::Scroll) {
                    itemLine += "  (Teleports to Safe Tile)";
                }

                if (it.equipped) {
                    SetTextColor(memDC, RGB(40, 230, 80)); // Green for equipped
                    itemLine += "   [EQUIPPED]";
                } else if (isSelected) {
                    SetTextColor(memDC, RGB(255, 255, 255)); // Bright white for selected
                } else {
                    SetTextColor(memDC, RGB(200, 210, 225));
                }

                TextOutA(memDC, modalX + 20, itemY + 2, itemLine.c_str(), static_cast<int>(itemLine.size()));
                itemY += 24;
            }
        }

        // Footer instructions
        SetTextColor(memDC, RGB(140, 210, 255));
        std::string foot = "[Up/Down/Click] Select  |  [Enter/Space/L-Click] Use/Equip  |  [D/Del/R-Click] Drop  |  [I/ESC] Close";
        RECT rFoot = { modalX, modalY + modalH - 28, modalX + modalW, modalY + modalH - 6 };
        DrawTextA(memDC, foot.c_str(), static_cast<int>(foot.size()), &rFoot, DT_CENTER | DT_SINGLELINE);

        DeleteObject(hbrModalBg);
        DeleteObject(hpenModalBorder);
        DeleteObject(hbrSelectedRow);
        DeleteObject(hpenSelectedBorder);
    }

    // 7. Game Over / Victory Overlay
    if (m_gameOver || m_victory) {
        int bannerW = 460;
        int bannerH = 110;
        int bannerX = (width - bannerW) / 2;
        int bannerY = 120;

        HBRUSH hbrOverlayBg = CreateSolidBrush(m_gameOver ? RGB(35, 10, 12) : RGB(10, 35, 20));
        HPEN hpenOverlayBorder = CreatePen(PS_SOLID, 3, m_gameOver ? RGB(230, 40, 40) : RGB(255, 215, 0));

        HPEN oldP = static_cast<HPEN>(SelectObject(memDC, hpenOverlayBorder));
        HBRUSH oldB = static_cast<HBRUSH>(SelectObject(memDC, hbrOverlayBg));
        Rectangle(memDC, bannerX, bannerY, bannerX + bannerW, bannerY + bannerH);
        SelectObject(memDC, oldB);
        SelectObject(memDC, oldP);

        SelectObject(memDC, hFontBanner);
        SetTextColor(memDC, m_gameOver ? RGB(255, 60, 60) : RGB(255, 220, 50));
        std::string title = m_gameOver ? "*** GAME OVER ***" : "*** VICTORY! ***";
        RECT rTitle = { bannerX, bannerY + 14, bannerX + bannerW, bannerY + 50 };
        DrawTextA(memDC, title.c_str(), static_cast<int>(title.size()), &rTitle, DT_CENTER | DT_SINGLELINE);

        SelectObject(memDC, hFontRegular);
        SetTextColor(memDC, RGB(240, 240, 240));
        std::string subtext = m_gameOver ?
            "You have fallen! Press [R] to Restart or [Q] to Quit." :
            "You retrieved the Amulet and escaped! Press [R] to Restart.";
        RECT rSub = { bannerX + 10, bannerY + 62, bannerX + bannerW - 10, bannerY + 95 };
        DrawTextA(memDC, subtext.c_str(), static_cast<int>(subtext.size()), &rSub, DT_CENTER | DT_SINGLELINE);

        DeleteObject(hbrOverlayBg);
        DeleteObject(hpenOverlayBorder);
    }

    // Cleanup GDI Objects
    SelectObject(memDC, oldFont);

    DeleteObject(hFontRegular);
    DeleteObject(hFontTile);
    DeleteObject(hFontBanner);

    DeleteObject(hbrFloorVis);
    DeleteObject(hbrFloorDim);
    DeleteObject(hbrWallCenterVis);
    DeleteObject(hbrWallCenterDim);
    DeleteObject(hpenLightBevelVis);
    DeleteObject(hpenLightBevelDim);
    DeleteObject(hpenDarkBevelVis);
    DeleteObject(hpenDarkBevelDim);

    DeleteObject(hbrDoorVis);
    DeleteObject(hbrDoorDim);
    DeleteObject(hpenDoorFrame);

    DeleteObject(hpenStairsVis);
    DeleteObject(hpenStairsDim);

    DeleteObject(hbrPotion);
    DeleteObject(hbrWeapon);
    DeleteObject(hbrArmor);
    DeleteObject(hbrScroll);
    DeleteObject(hbrGold);

    DeleteObject(hbrGoblin);
    DeleteObject(hbrSkeleton);
    DeleteObject(hbrOrc);
    DeleteObject(hbrDragon);
    DeleteObject(hpenDarkEntity);

    DeleteObject(hbrPlayer);
    DeleteObject(hpenPlayerGlow);

    DeleteObject(hbrGreen);
    DeleteObject(hbrYellow);
    DeleteObject(hbrRed);
    DeleteObject(hbrDarkRed);
    DeleteObject(hbrExpBg);
    DeleteObject(hbrExpFg);

    DeleteObject(hbrHudBg);
    DeleteObject(hpenAccent);
#else
    (void)hdcVoid;
    (void)width;
    (void)height;
#endif
}
