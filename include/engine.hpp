#pragma once

#include "map.hpp"
#include "ai.hpp"
#include <string>
#include <vector>
#include <cstdint>

enum class ItemCategory {
    Weapon,
    Armor,
    Potion,
    Scroll
};

struct Item {
    int id{0};
    std::string name{"Health Potion"};
    ItemCategory category{ItemCategory::Potion};
    char symbol{'!'};
    std::string colorCode{"\033[1;35m"}; // ANSI escape
    Point pos{0, 0};
    int healAmount{0};
    int atkBonus{0};
    int defBonus{0};
    bool equipped{false};
    bool pickedUp{false};
};

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
    static constexpr size_t MAX_INVENTORY_CAPACITY = 10;

    Engine();
    ~Engine();

    void init(unsigned int seed = 1337);
    void restart();

    // Presentation modes
    void runInteractive(); // Terminal ANSI mode
    void runGui();         // Native Win32 GDI 2D Tile GUI mode
    void runHeadless(int maxTicks, unsigned int seed = 1337);

    // Game loop actions
    bool stepPlayer(int dx, int dy);
    bool descendStairs();
    void waitTurn();
    void updateMonsters();
    void attackEntity(Entity& attacker, Entity& defender);
    void addMessage(const std::string& msg);
    void addExp(int amount);
    void checkItemPickup();

    // Inventory & Equipment
    bool useOrEquipItem(size_t index);
    bool dropItem(size_t index);
    bool isInventoryOpen() const { return m_inventoryOpen; }
    void toggleInventory() { m_inventoryOpen = !m_inventoryOpen; }
    void setInventoryOpen(bool open) { m_inventoryOpen = open; }

    size_t getInventorySelectedIndex() const { return m_inventorySelectedIndex; }
    void setInventorySelectedIndex(size_t idx);
    void selectPrevInventoryItem();
    void selectNextInventoryItem();
    bool useOrEquipSelectedItem();
    bool dropSelectedItem();
    void handleInventoryClick(int mouseX, int mouseY, bool rightClick);

    int getTotalPlayerAttack() const;
    int getTotalPlayerDefense() const;
    const Item* getEquippedWeapon() const;
    const Item* getEquippedArmor() const;

    // Rendering
    void render() const;
    std::string renderToString() const;
    void renderGui(void* hdc, int width, int height);

    // State inspection
    bool isRunning() const { return m_running; }
    bool isGameOver() const { return m_gameOver; }
    bool isVictory() const { return m_victory; }
    int getTurnCount() const { return m_turnCount; }
    int getFloor() const { return m_floor; }
    int getSafeRestCount() const { return m_safeRestCount; }

    int getPlayerLevel() const { return m_playerLevel; }
    int getPlayerExp() const { return m_playerExp; }
    int getPlayerExpToNext() const { return m_playerExpToNext; }

    const Entity& getPlayer() const { return m_player; }
    Entity& getPlayer() { return m_player; }
    const std::vector<Entity>& getMonsters() const { return m_monsters; }
    std::vector<Entity>& getMonsters() { return m_monsters; }
    const std::vector<Item>& getItems() const { return m_items; }
    std::vector<Item>& getItems() { return m_items; }
    const std::vector<Item>& getInventory() const { return m_inventory; }
    std::vector<Item>& getInventory() { return m_inventory; }
    const Map& getMap() const { return m_map; }
    const FogOfWar& getFogOfWar() const { return m_fow; }
    const FogOfWar& getFov() const { return m_fow; }
    const std::vector<std::string>& getMessageLog() const { return m_messageLog; }

private:
    Entity m_player;
    std::vector<Entity> m_monsters;
    std::vector<Item> m_items;
    std::vector<Item> m_inventory;
    Map m_map;
    FogOfWar m_fow;

    bool m_running{true};
    bool m_gameOver{false};
    bool m_victory{false};
    bool m_inventoryOpen{false};
    size_t m_inventorySelectedIndex{0};
    int m_turnCount{0};
    int m_floor{1};
    int m_safeRestCount{0};
    int m_playerLevel{1};
    int m_playerExp{0};
    int m_playerExpToNext{20};
    unsigned int m_seed{1337};
    std::vector<std::string> m_messageLog;

    unsigned long m_originalConsoleMode{0};

    void initConsole();
    void restoreConsole();
    void spawnMonsters(int count);
    void spawnItems(int count);
    void handleKey(int ch);
    void handleSpecialKey(int code);
};
