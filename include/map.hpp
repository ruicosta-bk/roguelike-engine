#pragma once

#include <cstdint>
#include <vector>

struct Point {
    int x{0};
    int y{0};

    bool operator==(const Point& o) const {
        return x == o.x && y == o.y;
    }

    bool operator!=(const Point& o) const {
        return !(*this == o);
    }

    bool operator<(const Point& o) const {
        if (x != o.x) return x < o.x;
        return y < o.y;
    }
};

enum class TileType : uint8_t {
    Wall,
    Floor,
    DoorClosed,
    DoorOpen,
    StairsDown
};

struct Room {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    Point center() const {
        return Point{ x + width / 2, y + height / 2 };
    }

    bool intersects(const Room& other) const {
        return (x <= other.x + other.width && x + width >= other.x &&
                y <= other.y + other.height && y + height >= other.y);
    }
};

class Map {
public:
    Map(int width = 80, int height = 25);

    // Procedural generation with guaranteed connectivity, rooms, corridors, doors, and stairs down
    void generate(unsigned int seed = 1337);

    TileType getTile(int x, int y) const;
    void setTile(int x, int y, TileType tile);

    bool isInBounds(int x, int y) const;
    bool isWalkable(int x, int y) const;    // Floor, DoorOpen, StairsDown
    bool isTransparent(int x, int y) const; // Floor, DoorOpen, StairsDown

    Point getPlayerSpawn() const;
    std::vector<Point> getMonsterSpawns(int count) const;
    std::vector<Point> getItemSpawns(int count) const;

    int getWidth() const;
    int getHeight() const;
    const std::vector<Room>& getRooms() const;

private:
    int m_width;
    int m_height;
    unsigned int m_seed{1337};
    std::vector<TileType> m_tiles;
    std::vector<Room> m_rooms;
    Point m_playerSpawn{0, 0};
    Point m_stairsDownPos{0, 0};

    bool isInsideAnyRoom(int x, int y) const;
    void carveRoom(const Room& room);
    void carveCorridor(Point p1, Point p2, bool horizontalFirst);
    void placeDoors();
};
