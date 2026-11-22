// Shortest include-path query. Pure, explicit BFS state machine.
#pragma once

#include <string>
#include <vector>

#include "types.hpp"

namespace knot::core {

enum class PathMark { Pending, Seen };

struct PathState {
    std::vector<PathMark> marks;
    std::vector<std::vector<std::string>> frontier;
    std::vector<std::string> found;
    bool done = false;
};

PathState path_init(const IncludeGraph& g, const std::string& from);
// One pure BFS micro-step. Expands the oldest path in the frontier.
PathState path_step(PathState s, const IncludeGraph& g, const std::string& target);

// Shortest path from -> to, empty when unreachable. Deterministic on ties.
std::vector<std::string> shortest_path(const IncludeGraph& g, const std::string& from,
                                       const std::string& to);

// Shortest path from any translation unit to `target`; ties break by path.
std::vector<std::string> best_path_to(const IncludeGraph& g, const std::string& target);

// Resolve a user-supplied node name against the graph: exact match first,
// then a unique path-suffix match. Returns the input unchanged when ambiguous.
std::string resolve_node(const IncludeGraph& g, const std::string& name);

std::string render_path_text(const std::string& target, const std::vector<std::string>& path);
std::string render_path_json(const std::string& target, const std::vector<std::string>& path);

}  // namespace knot::core
