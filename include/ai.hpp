#pragma once

#include "map.hpp"
#include <vector>
#include <cstdint>

// Bresenham Line of Sight utility
class LineOfSight {
public:
    // Returns true if there is an unobstructed line of sight from 'from' to 'to'.
    // Intermediate tiles must satisfy map.isTransparent(x, y).
    // The endpoint 'to' is visible even if it is opaque (e.g. looking at a wall).
    static bool hasLineOfSight(const Map& map, Point from, Point to);

    // Returns all points along the Bresenham line from 'from' to 'to' (inclusive).
    static std::vector<Point> getLine(Point from, Point to);
};

// Fog of War maintaining 2D grid state for explored and visible tiles
class FogOfWar {
public:
    FogOfWar(int width = 80, int height = 25);

    void init(int width, int height);

    // Resets visibility state of all tiles to false (explored remains unchanged)
    void resetVisibility();

    // Computes visible tiles within radius from origin using Bresenham line of sight.
    // Visible tiles are marked as both visible and explored.
    // If resetVisible is true, resets visibility of prior frame first.
    void computeFOV(const Map& map, Point origin, int radius = 8, bool resetVisible = true);

    bool isExplored(int x, int y) const;
    bool isVisible(int x, int y) const;

    void setExplored(int x, int y, bool val = true);
    void setVisible(int x, int y, bool val = true);

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    int m_width{0};
    int m_height{0};
    std::vector<uint8_t> m_explored;
    std::vector<uint8_t> m_visible;

    bool isInBounds(int x, int y) const {
        return x >= 0 && x < m_width && y >= 0 && y < m_height;
    }

    int toIndex(int x, int y) const {
        return y * m_width + x;
    }
};

// A* Pathfinding
class Pathfinding {
public:
    // A* algorithm with Manhattan / Euclidean heuristic.
    // Checks map.isWalkable and blocker collisions.
    // Returns sequence of steps from start to target (excluding start, including target if reachable),
    // or empty vector if no path exists.
    static std::vector<Point> findPath(
        const Map& map,
        Point start,
        Point target,
        const std::vector<Point>& blockers = {},
        bool allowDiagonal = false
    );
};

// AI Action definitions
enum class AIActionType {
    Idle,
    Move,
    Attack
};

struct AIAction {
    AIActionType type{AIActionType::Idle};
    Point targetPos{0, 0}; // If Move: adjacent tile to step onto. If Attack: target's tile.
};

// Monster AI decision helper
struct MonsterAI {
    // Determines monster's next action:
    // - If target is adjacent, return Attack.
    // - If target is within visionRange (and optionally visible / reachable), chase using Pathfinding.
    // - Otherwise return Idle.
    static AIAction decideAction(
        const Map& map,
        Point monsterPos,
        Point targetPos,
        const std::vector<Point>& blockers = {},
        int visionRange = 8,
        bool requireLineOfSight = false,
        bool allowDiagonal = false
    );

    // Checks adjacency between two points
    static bool isAdjacent(Point a, Point b, bool allowDiagonal = false);
};
