#include "map.hpp"

#include <random>
#include <algorithm>
#include <cmath>

Map::Map(int width, int height)
    : m_width(std::max(10, width)),
      m_height(std::max(10, height)),
      m_tiles(m_width * m_height, TileType::Wall) {
    generate(1337);
}

TileType Map::getTile(int x, int y) const {
    if (!isInBounds(x, y)) {
        return TileType::Wall;
    }
    return m_tiles[y * m_width + x];
}

void Map::setTile(int x, int y, TileType tile) {
    if (isInBounds(x, y)) {
        m_tiles[y * m_width + x] = tile;
    }
}

bool Map::isInBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

bool Map::isWalkable(int x, int y) const {
    if (!isInBounds(x, y)) {
        return false;
    }
    TileType t = getTile(x, y);
    return t == TileType::Floor || t == TileType::DoorOpen || t == TileType::StairsDown;
}

bool Map::isTransparent(int x, int y) const {
    if (!isInBounds(x, y)) {
        return false;
    }
    TileType t = getTile(x, y);
    return t == TileType::Floor || t == TileType::DoorOpen || t == TileType::StairsDown;
}

Point Map::getPlayerSpawn() const {
    return m_playerSpawn;
}

int Map::getWidth() const {
    return m_width;
}

int Map::getHeight() const {
    return m_height;
}

const std::vector<Room>& Map::getRooms() const {
    return m_rooms;
}

bool Map::isInsideAnyRoom(int x, int y) const {
    for (const auto& room : m_rooms) {
        if (x >= room.x && x < room.x + room.width &&
            y >= room.y && y < room.y + room.height) {
            return true;
        }
    }
    return false;
}

void Map::carveRoom(const Room& room) {
    for (int y = room.y; y < room.y + room.height; ++y) {
        for (int x = room.x; x < room.x + room.width; ++x) {
            setTile(x, y, TileType::Floor);
        }
    }
}

void Map::carveCorridor(Point p1, Point p2, bool horizontalFirst) {
    if (horizontalFirst) {
        int minX = std::min(p1.x, p2.x);
        int maxX = std::max(p1.x, p2.x);
        for (int x = minX; x <= maxX; ++x) {
            setTile(x, p1.y, TileType::Floor);
        }
        int minY = std::min(p1.y, p2.y);
        int maxY = std::max(p1.y, p2.y);
        for (int y = minY; y <= maxY; ++y) {
            setTile(p2.x, y, TileType::Floor);
        }
    } else {
        int minY = std::min(p1.y, p2.y);
        int maxY = std::max(p1.y, p2.y);
        for (int y = minY; y <= maxY; ++y) {
            setTile(p1.x, y, TileType::Floor);
        }
        int minX = std::min(p1.x, p2.x);
        int maxX = std::max(p1.x, p2.x);
        for (int x = minX; x <= maxX; ++x) {
            setTile(x, p2.y, TileType::Floor);
        }
    }
}

void Map::placeDoors() {
    for (int y = 1; y < m_height - 1; ++y) {
        for (int x = 1; x < m_width - 1; ++x) {
            if (getTile(x, y) != TileType::Floor) continue;
            if (isInsideAnyRoom(x, y)) continue;

            // Horizontal door: Wall above and below, walkable passage left and right
            bool wallNorthSouth = (getTile(x, y - 1) == TileType::Wall && getTile(x, y + 1) == TileType::Wall);
            bool passageEastWest = (getTile(x - 1, y) != TileType::Wall && getTile(x + 1, y) != TileType::Wall);
            bool roomBoundaryH = (isInsideAnyRoom(x - 1, y) || isInsideAnyRoom(x + 1, y));

            bool isHDoor = wallNorthSouth && passageEastWest && roomBoundaryH;

            // Vertical door: Wall left and right, walkable passage above and below
            bool wallEastWest = (getTile(x - 1, y) == TileType::Wall && getTile(x + 1, y) == TileType::Wall);
            bool passageNorthSouth = (getTile(x, y - 1) != TileType::Wall && getTile(x, y + 1) != TileType::Wall);
            bool roomBoundaryV = (isInsideAnyRoom(x, y - 1) || isInsideAnyRoom(x, y + 1));

            bool isVDoor = wallEastWest && passageNorthSouth && roomBoundaryV;

            if (isHDoor || isVDoor) {
                // Check that no neighbor in 4-neighborhood is already a door
                bool adjacentDoor = false;
                const int dx[] = {1, -1, 0, 0};
                const int dy[] = {0, 0, 1, -1};
                for (int i = 0; i < 4; ++i) {
                    TileType nt = getTile(x + dx[i], y + dy[i]);
                    if (nt == TileType::DoorClosed || nt == TileType::DoorOpen) {
                        adjacentDoor = true;
                        break;
                    }
                }
                if (!adjacentDoor) {
                    setTile(x, y, TileType::DoorOpen);
                }
            }
        }
    }
}

void Map::generate(unsigned int seed) {
    m_seed = seed;
    std::mt19937 rng(seed);

    m_tiles.assign(m_width * m_height, TileType::Wall);
    m_rooms.clear();

    const int maxRooms = 12;
    const int minRoomSize = 5;
    const int maxRoomWidth = 12;
    const int maxRoomHeight = 8;
    const int maxAttempts = 150;

    std::uniform_int_distribution<int> distW(minRoomSize, maxRoomWidth);
    std::uniform_int_distribution<int> distH(minRoomSize, maxRoomHeight);

    for (int attempt = 0; attempt < maxAttempts && static_cast<int>(m_rooms.size()) < maxRooms; ++attempt) {
        int w = distW(rng);
        int h = distH(rng);
        if (m_width - w - 2 <= 1 || m_height - h - 2 <= 1) continue;

        std::uniform_int_distribution<int> distX(1, m_width - w - 2);
        std::uniform_int_distribution<int> distY(1, m_height - h - 2);

        Room newRoom{distX(rng), distY(rng), w, h};

        bool failed = false;
        for (const auto& other : m_rooms) {
            if (newRoom.intersects(other)) {
                failed = true;
                break;
            }
        }

        if (!failed) {
            carveRoom(newRoom);

            if (!m_rooms.empty()) {
                Point prevCenter = m_rooms.back().center();
                Point newCenter = newRoom.center();
                bool horizFirst = (rng() % 2 == 0);
                carveCorridor(prevCenter, newCenter, horizFirst);
            }

            m_rooms.push_back(newRoom);
        }
    }

    // Fallback if too few rooms placed
    if (m_rooms.size() < 2) {
        m_tiles.assign(m_width * m_height, TileType::Wall);
        m_rooms.clear();

        Room r1{2, 2, std::min(8, m_width - 4), std::min(6, m_height - 4)};
        Room r2{std::max(2, m_width - 12), std::max(2, m_height - 10), std::min(8, m_width - 4), std::min(6, m_height - 4)};
        carveRoom(r1);
        m_rooms.push_back(r1);
        if (!r1.intersects(r2)) {
            carveRoom(r2);
            carveCorridor(r1.center(), r2.center(), true);
            m_rooms.push_back(r2);
        }
    }

    // Additional loop connections for natural dungeon layout
    if (m_rooms.size() >= 4) {
        bool horizFirst = (rng() % 2 == 0);
        carveCorridor(m_rooms.front().center(), m_rooms.back().center(), horizFirst);
        if (m_rooms.size() >= 6) {
            size_t mid = m_rooms.size() / 2;
            carveCorridor(m_rooms[1].center(), m_rooms[mid].center(), !horizFirst);
        }
    }

    // Player spawn: center of the first room
    m_playerSpawn = m_rooms[0].center();

    // Stairs down: furthest room from player spawn
    size_t furthestIdx = (m_rooms.size() > 1) ? 1 : 0;
    int maxDistSq = -1;
    for (size_t i = 1; i < m_rooms.size(); ++i) {
        Point c = m_rooms[i].center();
        int dx = c.x - m_playerSpawn.x;
        int dy = c.y - m_playerSpawn.y;
        int distSq = dx * dx + dy * dy;
        if (distSq > maxDistSq) {
            maxDistSq = distSq;
            furthestIdx = i;
        }
    }
    m_stairsDownPos = m_rooms[furthestIdx].center();
    setTile(m_stairsDownPos.x, m_stairsDownPos.y, TileType::StairsDown);

    // Place doors at room/corridor junctions
    placeDoors();
}

std::vector<Point> Map::getMonsterSpawns(int count) const {
    if (count <= 0) {
        return {};
    }

    std::vector<Point> candidates;

    // Collect walkable floor tiles across non-starting rooms
    for (size_t i = 1; i < m_rooms.size(); ++i) {
        const auto& room = m_rooms[i];
        for (int y = room.y; y < room.y + room.height; ++y) {
            for (int x = room.x; x < room.x + room.width; ++x) {
                if (getTile(x, y) == TileType::Floor && Point{x, y} != m_stairsDownPos) {
                    candidates.push_back(Point{x, y});
                }
            }
        }
    }

    // Fallback if no other rooms
    if (candidates.empty()) {
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                if (getTile(x, y) == TileType::Floor &&
                    Point{x, y} != m_playerSpawn &&
                    Point{x, y} != m_stairsDownPos) {
                    candidates.push_back(Point{x, y});
                }
            }
        }
    }

    std::mt19937 rng(m_seed + 10007);
    std::shuffle(candidates.begin(), candidates.end(), rng);

    size_t resultCount = std::min(static_cast<size_t>(count), candidates.size());
    return std::vector<Point>(candidates.begin(), candidates.begin() + resultCount);
}

std::vector<Point> Map::getItemSpawns(int count) const {
    if (count <= 0) {
        return {};
    }

    std::vector<Point> candidates;

    // Collect walkable floor tiles across all rooms, excluding player spawn and stairs down
    for (const auto& room : m_rooms) {
        for (int y = room.y; y < room.y + room.height; ++y) {
            for (int x = room.x; x < room.x + room.width; ++x) {
                Point p{x, y};
                if (getTile(x, y) == TileType::Floor && p != m_playerSpawn && p != m_stairsDownPos) {
                    candidates.push_back(p);
                }
            }
        }
    }

    // Fallback if no room floor candidates
    if (candidates.empty()) {
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                Point p{x, y};
                if (getTile(x, y) == TileType::Floor && p != m_playerSpawn && p != m_stairsDownPos) {
                    candidates.push_back(p);
                }
            }
        }
    }

    std::mt19937 rng(m_seed + 20011);
    std::shuffle(candidates.begin(), candidates.end(), rng);

    size_t resultCount = std::min(static_cast<size_t>(count), candidates.size());
    return std::vector<Point>(candidates.begin(), candidates.begin() + resultCount);
}
