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
    m_messageLog.clear();

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

    addMessage("Welcome to the Dungeons of Doom! [WASD/Arrows: Move, R: Restart, Q: Quit]");
    spawnMonsters(6);
    spawnItems(4);
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
        if (roll < 45) {
            monster.name = "Goblin";
            monster.symbol = 'g';
            monster.colorCode = "\033[1;32m"; // Bright green
            monster.hp = 8 + (m_floor - 1) * 2;
            monster.maxHp = monster.hp;
            monster.attack = 3 + (m_floor - 1);
            monster.defense = 1;
        } else if (roll < 75) {
            monster.name = "Skeleton";
            monster.symbol = 's';
            monster.colorCode = "\033[1;37m"; // Bright white
            monster.hp = 12 + (m_floor - 1) * 2;
            monster.maxHp = monster.hp;
            monster.attack = 4 + (m_floor - 1);
            monster.defense = 1;
        } else if (roll < 90) {
            monster.name = "Orc";
            monster.symbol = 'o';
            monster.colorCode = "\033[1;31m"; // Bright red
            monster.hp = 16 + (m_floor - 1) * 3;
            monster.maxHp = monster.hp;
            monster.attack = 5 + (m_floor - 1);
            monster.defense = 2;
        } else {
            monster.name = "Dragon";
            monster.symbol = 'D';
            monster.colorCode = "\033[1;35m"; // Bright magenta
            monster.hp = 25 + (m_floor - 1) * 4;
            monster.maxHp = monster.hp;
            monster.attack = 7 + (m_floor - 1);
            monster.defense = 3;
        }
        m_monsters.push_back(monster);
    }
}

void Engine::spawnItems(int count) {
    m_items.clear();
    auto spawnPoints = m_map.getItemSpawns(count);
    int idCounter = 1;

    for (const auto& pt : spawnPoints) {
        Item item;
        item.id = idCounter++;
        item.name = "Health Potion";
        item.symbol = '!';
        item.colorCode = "\033[1;35m"; // Bright magenta
        item.pos = pt;
        item.healAmount = 12;
        item.pickedUp = false;
        m_items.push_back(item);
    }
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
            item.pickedUp = true;
            m_player.hp = std::min(m_player.maxHp, m_player.hp + item.healAmount);
            addMessage("You drank a Health Potion (+" + std::to_string(item.healAmount) + " HP)!");
        }
    }
}

void Engine::attackEntity(Entity& attacker, Entity& defender) {
    int damage = std::max(1, attacker.attack - defender.defense);
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
            if (defender.name == "Goblin") expVal = 10;
            else if (defender.name == "Skeleton") expVal = 15;
            else if (defender.name == "Orc") expVal = 25;
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
        spawnItems(4);
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

        AIAction action = MonsterAI::decideAction(m_map, monster.pos, m_player.pos, blockers, 8, false, false);

        if (action.type == AIActionType::Attack) {
            attackEntity(monster, m_player);
            if (!m_player.isAlive()) {
                m_gameOver = true;
                m_running = false;
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

std::string Engine::renderToString() const {
    std::string out;
    out.reserve(80 * 25 * 4 + 1024);

    // Cursor reset to top-left to avoid flicker
    out += "\033[H";

    int width = m_map.getWidth();
    int height = m_map.getHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool visible = m_fow.isVisible(x, y);
            bool explored = m_fow.isExplored(x, y);

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

                // Check consumable item on visible tile
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

    // HUD Line 2: HP Bar, Atk, Def
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

    out += " HP: " + hpColor + "[" + std::string(static_cast<size_t>(filled), '=') +
           std::string(static_cast<size_t>(barWidth - filled), ' ') + "] ";
    out += std::to_string(curHp) + "/" + std::to_string(maxHp) + "\033[0m | ";
    out += "Atk: \033[1;35m" + std::to_string(m_player.attack) + "\033[0m | ";
    out += "Def: \033[1;35m" + std::to_string(m_player.defense) + "\033[0m\033[K\n";

    // Status / Controls
    if (m_gameOver) {
        out += " \033[1;31m*** GAME OVER - Press [R] to Restart, [Q] to Quit ***\033[0m\033[K\n";
    } else if (m_victory) {
        out += " \033[1;32m*** VICTORY! Dungeon Cleared! Press [R] to Restart, [Q] to Quit ***\033[0m\033[K\n";
    } else {
        out += " \033[90mControls: [WASD/Arrows] Move | [.] Rest | [>] Stairs | [R] Restart | [Q] Quit\033[0m\033[K\n";
    }

    // Message Log (last 5 messages)
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

        // 3. If low HP and a potion is visible/reachable, try pathfinding towards potion
        if (m_player.hp < m_player.maxHp / 2) {
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
