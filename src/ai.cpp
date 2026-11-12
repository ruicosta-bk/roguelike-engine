#include "ai.hpp"

#include <queue>
#include <algorithm>
#include <cmath>
#include <cstdlib>

// ==========================================
// LineOfSight Implementation
// ==========================================

std::vector<Point> LineOfSight::getLine(Point from, Point to) {
    std::vector<Point> points;
    int dx = std::abs(to.x - from.x);
    int dy = std::abs(to.y - from.y);
    int sx = (from.x < to.x) ? 1 : -1;
    int sy = (from.y < to.y) ? 1 : -1;
    int err = dx - dy;

    Point curr = from;
    while (true) {
        points.push_back(curr);
        if (curr == to) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            curr.x += sx;
        }
        if (e2 < dx) {
            err += dx;
            curr.y += sy;
        }
    }
    return points;
}

bool LineOfSight::hasLineOfSight(const Map& map, Point from, Point to) {
    if (!map.isInBounds(from.x, from.y) || !map.isInBounds(to.x, to.y)) {
        return false;
    }
    if (from == to) {
        return true;
    }

    int dx = std::abs(to.x - from.x);
    int dy = std::abs(to.y - from.y);
    int sx = (from.x < to.x) ? 1 : -1;
    int sy = (from.y < to.y) ? 1 : -1;
    int err = dx - dy;

    Point curr = from;
    while (true) {
        if (curr == to) {
            return true;
        }
        // Intermediate tiles must be transparent
        if (curr != from && !map.isTransparent(curr.x, curr.y)) {
            return false;
        }
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            curr.x += sx;
        }
        if (e2 < dx) {
            err += dx;
            curr.y += sy;
        }
    }
}

// ==========================================
// FogOfWar Implementation
// ==========================================

FogOfWar::FogOfWar(int width, int height) {
    init(width, height);
}

void FogOfWar::init(int width, int height) {
    m_width = width;
    m_height = height;
    m_explored.assign(m_width * m_height, 0);
    m_visible.assign(m_width * m_height, 0);
}

void FogOfWar::resetVisibility() {
    std::fill(m_visible.begin(), m_visible.end(), 0);
}

void FogOfWar::computeFOV(const Map& map, Point origin, int radius, bool resetVisible) {
    if (m_width != map.getWidth() || m_height != map.getHeight()) {
        init(map.getWidth(), map.getHeight());
    }
    if (resetVisible) {
        resetVisibility();
    }
    if (!map.isInBounds(origin.x, origin.y)) {
        return;
    }

    // Origin is always visible and explored
    setVisible(origin.x, origin.y, true);
    setExplored(origin.x, origin.y, true);

    int r2 = radius * radius;
    int minX = std::max(0, origin.x - radius);
    int maxX = std::min(m_width - 1, origin.x + radius);
    int minY = std::max(0, origin.y - radius);
    int maxY = std::min(m_height - 1, origin.y + radius);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            int dx = x - origin.x;
            int dy = y - origin.y;
            if (dx * dx + dy * dy <= r2) {
                if (LineOfSight::hasLineOfSight(map, origin, Point{ x, y })) {
                    setVisible(x, y, true);
                    setExplored(x, y, true);
                }
            }
        }
    }
}

bool FogOfWar::isExplored(int x, int y) const {
    if (!isInBounds(x, y)) return false;
    return m_explored[toIndex(x, y)] != 0;
}

bool FogOfWar::isVisible(int x, int y) const {
    if (!isInBounds(x, y)) return false;
    return m_visible[toIndex(x, y)] != 0;
}

void FogOfWar::setExplored(int x, int y, bool val) {
    if (isInBounds(x, y)) {
        m_explored[toIndex(x, y)] = val ? 1 : 0;
    }
}

void FogOfWar::setVisible(int x, int y, bool val) {
    if (isInBounds(x, y)) {
        m_visible[toIndex(x, y)] = val ? 1 : 0;
    }
}

// ==========================================
// Pathfinding Implementation
// ==========================================

std::vector<Point> Pathfinding::findPath(
    const Map& map,
    Point start,
    Point target,
    const std::vector<Point>& blockers,
    bool allowDiagonal
) {
    int width = map.getWidth();
    int height = map.getHeight();

    if (width <= 0 || height <= 0) return {};
    if (!map.isInBounds(start.x, start.y) || !map.isInBounds(target.x, target.y)) return {};
    if (start == target) return {};
    if (!map.isWalkable(target.x, target.y)) return {};

    std::vector<uint8_t> blocked(width * height, 0);
    for (const auto& b : blockers) {
        if (map.isInBounds(b.x, b.y) && b != target && b != start) {
            blocked[b.y * width + b.x] = 1;
        }
    }

    const int INF = 1000000000;
    std::vector<int> gScore(width * height, INF);
    std::vector<int> parent(width * height, -1);
    std::vector<uint8_t> closed(width * height, 0);

    struct AStarNode {
        int fScore;
        int gScore;
        int index;
        bool operator>(const AStarNode& other) const {
            return fScore > other.fScore;
        }
    };

    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;

    int startIndex = start.y * width + start.x;
    int targetIndex = target.y * width + target.x;

    auto heuristic = [&](int x, int y) -> int {
        int dx = std::abs(x - target.x);
        int dy = std::abs(y - target.y);
        if (allowDiagonal) {
            return 10 * (dx + dy) - 6 * std::min(dx, dy); // Octile distance
        }
        return dx + dy; // Manhattan distance
    };

    gScore[startIndex] = 0;
    openSet.push({ heuristic(start.x, start.y), 0, startIndex });

    static const int DX4[4] = { 0, 0, 1, -1 };
    static const int DY4[4] = { -1, 1, 0, 0 };

    static const int DX8[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    static const int DY8[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };

    int numDirs = allowDiagonal ? 8 : 4;

    while (!openSet.empty()) {
        AStarNode cur = openSet.top();
        openSet.pop();

        if (closed[cur.index]) continue;
        closed[cur.index] = 1;

        if (cur.index == targetIndex) break;

        int cx = cur.index % width;
        int cy = cur.index / width;

        for (int d = 0; d < numDirs; ++d) {
            int nx = cx + (allowDiagonal ? DX8[d] : DX4[d]);
            int ny = cy + (allowDiagonal ? DY8[d] : DY4[d]);

            if (!map.isInBounds(nx, ny)) continue;
            if (!map.isWalkable(nx, ny)) continue;

            int nextIndex = ny * width + nx;
            if (blocked[nextIndex]) continue;
            if (closed[nextIndex]) continue;

            // Diagonal corner-cutting check
            if (allowDiagonal && d >= 4) {
                if (!map.isWalkable(cx, ny) || !map.isWalkable(nx, cy)) {
                    continue;
                }
            }

            int stepCost = (allowDiagonal && d >= 4) ? 14 : (allowDiagonal ? 10 : 1);
            int tentativeG = gScore[cur.index] + stepCost;

            if (tentativeG < gScore[nextIndex]) {
                gScore[nextIndex] = tentativeG;
                parent[nextIndex] = cur.index;
                int f = tentativeG + heuristic(nx, ny);
                openSet.push({ f, tentativeG, nextIndex });
            }
        }
    }

    if (parent[targetIndex] == -1) {
        return {};
    }

    std::vector<Point> path;
    int curr = targetIndex;
    while (curr != startIndex && curr != -1) {
        path.push_back(Point{ curr % width, curr / width });
        curr = parent[curr];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ==========================================
// MonsterAI Implementation
// ==========================================

bool MonsterAI::isAdjacent(Point a, Point b, bool allowDiagonal) {
    int dx = std::abs(a.x - b.x);
    int dy = std::abs(a.y - b.y);
    if (allowDiagonal) {
        return (dx <= 1 && dy <= 1) && (dx + dy > 0);
    } else {
        return (dx + dy == 1);
    }
}

Point MonsterAI::findFleeStep(
    const Map& map,
    Point monsterPos,
    Point targetPos,
    const std::vector<Point>& blockers
) {
    int curDistChebyshev = std::max(std::abs(monsterPos.x - targetPos.x), std::abs(monsterPos.y - targetPos.y));
    int curDistSq = (monsterPos.x - targetPos.x) * (monsterPos.x - targetPos.x) + 
                    (monsterPos.y - targetPos.y) * (monsterPos.y - targetPos.y);

    int bestDistSq = curDistSq;
    int bestDistChebyshev = curDistChebyshev;
    Point bestTile = monsterPos;
    bool found = false;

    static const int DX[4] = { 0, 0, 1, -1 };
    static const int DY[4] = { -1, 1, 0, 0 };

    for (int d = 0; d < 4; ++d) {
        int nx = monsterPos.x + DX[d];
        int ny = monsterPos.y + DY[d];
        Point next{ nx, ny };

        if (!map.isInBounds(nx, ny)) continue;
        if (!map.isWalkable(nx, ny)) continue;
        if (next == targetPos) continue;

        // Blocker check
        bool isBlocked = false;
        for (const auto& b : blockers) {
            if (b == next) {
                isBlocked = true;
                break;
            }
        }
        if (isBlocked) continue;

        int newDistChebyshev = std::max(std::abs(nx - targetPos.x), std::abs(ny - targetPos.y));
        int newDistSq = (nx - targetPos.x) * (nx - targetPos.x) + (ny - targetPos.y) * (ny - targetPos.y);

        // Prefer neighbor that strictly increases distance from target
        if (newDistChebyshev > bestDistChebyshev || 
           (newDistChebyshev == bestDistChebyshev && newDistSq > bestDistSq)) {
            bestDistChebyshev = newDistChebyshev;
            bestDistSq = newDistSq;
            bestTile = next;
            found = true;
        }
    }

    if (found) {
        return bestTile;
    }
    return monsterPos;
}

AIAction MonsterAI::decideAction(
    const Map& map,
    Point monsterPos,
    Point targetPos,
    const std::vector<Point>& blockers,
    int visionRange,
    MonsterArchetype archetype,
    int curHp,
    int maxHp,
    int minRangedDist,
    int maxRangedDist
) {
    int distChebyshev = std::max(std::abs(monsterPos.x - targetPos.x), std::abs(monsterPos.y - targetPos.y));
    int distSq = (monsterPos.x - targetPos.x) * (monsterPos.x - targetPos.x) + 
                 (monsterPos.y - targetPos.y) * (monsterPos.y - targetPos.y);

    // Target is out of vision threshold
    if (distSq > visionRange * visionRange && distChebyshev > visionRange) {
        return AIAction{ AIActionType::Idle, monsterPos };
    }

    // 1. Ranged Archetype (e.g. Skeleton Archer)
    if (archetype == MonsterArchetype::Ranged) {
        // Close range: kite / retreat away
        if (distChebyshev <= minRangedDist) {
            Point fleeStep = findFleeStep(map, monsterPos, targetPos, blockers);
            if (fleeStep != monsterPos) {
                return AIAction{ AIActionType::Move, fleeStep };
            }
            // Trapped and cannot retreat
            if (LineOfSight::hasLineOfSight(map, monsterPos, targetPos)) {
                return AIAction{ AIActionType::RangedAttack, targetPos };
            }
            if (isAdjacent(monsterPos, targetPos)) {
                return AIAction{ AIActionType::Attack, targetPos };
            }
            return AIAction{ AIActionType::Idle, monsterPos };
        }
        // Optimal shooting range with line of sight
        else if (distChebyshev <= maxRangedDist && LineOfSight::hasLineOfSight(map, monsterPos, targetPos)) {
            return AIAction{ AIActionType::RangedAttack, targetPos };
        }
        // Out of max shooting range or obstructed LOS: pathfind chase
        else {
            auto path = Pathfinding::findPath(map, monsterPos, targetPos, blockers, false);
            if (!path.empty()) {
                return AIAction{ AIActionType::Move, path[0] };
            }
            return AIAction{ AIActionType::Idle, monsterPos };
        }
    }

    // 2. Skirmisher Archetype (e.g. Goblin Rogue)
    if (archetype == MonsterArchetype::Skirmisher) {
        // Low HP (<= 50%): retreat
        if (curHp <= maxHp / 2) {
            Point fleeStep = findFleeStep(map, monsterPos, targetPos, blockers);
            if (fleeStep != monsterPos) {
                return AIAction{ AIActionType::Move, fleeStep };
            }
            // Trapped: fight if adjacent
            if (isAdjacent(monsterPos, targetPos)) {
                return AIAction{ AIActionType::Attack, targetPos };
            }
            return AIAction{ AIActionType::Idle, monsterPos };
        }

        // Healthy: standard pursuit and melee attack
        if (isAdjacent(monsterPos, targetPos)) {
            return AIAction{ AIActionType::Attack, targetPos };
        }

        auto path = Pathfinding::findPath(map, monsterPos, targetPos, blockers, false);
        if (!path.empty()) {
            return AIAction{ AIActionType::Move, path[0] };
        }
        return AIAction{ AIActionType::Idle, monsterPos };
    }

    // 3. Melee Archetype (e.g. Orc Warrior, Dragon)
    if (isAdjacent(monsterPos, targetPos)) {
        return AIAction{ AIActionType::Attack, targetPos };
    }

    auto path = Pathfinding::findPath(map, monsterPos, targetPos, blockers, false);
    if (!path.empty()) {
        return AIAction{ AIActionType::Move, path[0] };
    }

    return AIAction{ AIActionType::Idle, monsterPos };
}

AIAction MonsterAI::decideAction(
    const Map& map,
    Point monsterPos,
    Point targetPos,
    const std::vector<Point>& blockers,
    int visionRange,
    bool requireLineOfSight,
    bool allowDiagonal
) {
    (void)requireLineOfSight;
    (void)allowDiagonal;
    return decideAction(
        map,
        monsterPos,
        targetPos,
        blockers,
        visionRange,
        MonsterArchetype::Melee,
        10,
        10,
        2,
        5
    );
}
