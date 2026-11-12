#pragma once

#include "map.hpp"
#include "ai.hpp"
#include <string>
#include <vector>
#include <cstdint>

struct Entity {
    int id{0};
    std::string name;
    char symbol{'g'};
    std::string colorCode; // ANSI color escape, e.g. "\033[1;31m"
    Point pos{0, 0};
    int hp{10};
    int maxHp{10};
    int attack{3};
    int defense{1};
    bool isAlive() const { return hp > 0; }
};

class Engine {
public:
    Engine();
    ~Engine();

    void init(unsigned int seed = 1337);

    // Interactive loop
    void runInteractive();

    // Headless simulation mode
    void runHeadless(int maxTicks, unsigned int seed = 1337);

    // Game loop actions
    bool stepPlayer(int dx, int dy);
    bool descendStairs();
    void waitTurn();
    void updateMonsters();
    void attackEntity(Entity& attacker, Entity& defender);
    void addMessage(const std::string& msg);

    // Rendering
    void render() const;
    std::string renderToString() const;

    // State inspection
    bool isRunning() const { return m_running; }
    bool isGameOver() const { return m_gameOver; }
    bool isVictory() const { return m_victory; }
    int getTurnCount() const { return m_turnCount; }
    int getFloor() const { return m_floor; }

    const Entity& getPlayer() const { return m_player; }
    Entity& getPlayer() { return m_player; }
    const std::vector<Entity>& getMonsters() const { return m_monsters; }
    const Map& getMap() const { return m_map; }
    const FogOfWar& getFogOfWar() const { return m_fow; }
    const FogOfWar& getFov() const { return m_fow; }
    const std::vector<std::string>& getMessageLog() const { return m_messageLog; }

private:
    Entity m_player;
    std::vector<Entity> m_monsters;
    Map m_map;
    FogOfWar m_fow;

    bool m_running{true};
    bool m_gameOver{false};
    bool m_victory{false};
    int m_turnCount{0};
    int m_floor{1};
    unsigned int m_seed{1337};
    std::vector<std::string> m_messageLog;

    unsigned long m_originalConsoleMode{0};

    void initConsole();
    void restoreConsole();
    void spawnMonsters(int count);
    void handleKey(int ch);
    void handleSpecialKey(int code);
};
