// Pure data types for Knot's functional core.
// No I/O, no side effects, no platform headers.
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace knot::core {

// One `#include` directive, with its source location.
struct IncludeEdge {
    std::string from;    // normalized source path
    std::string header;  // header as written between < > or " "
    int line = 0;
};

// A source document handed to the core by the shell. Pure data.
struct SourceDoc {
    std::string path;
    std::string text;
};

// Adjacency-list view of the include graph. `ids` is sorted; `index` maps id
// to its position; `adj[i]` holds the sorted, unique ids included by `ids[i]`.
struct IncludeGraph {
    std::vector<std::string> ids;
    std::map<std::string, std::size_t> index;
    std::vector<std::vector<std::string>> adj;
};

struct Cycle {
    std::vector<std::string> nodes;  // a -> b -> ... -> a (first repeated at end)
};

struct Cost {
    std::string node;
    std::size_t direct = 0;
    std::size_t transitive = 0;
};

struct Hotspot {
    std::string header;
    std::size_t included_by = 0;
};

struct Recommendation {
    enum class Kind { BreakCycle, RemoveDuplicate, ForwardDeclare };

    Kind kind = Kind::RemoveDuplicate;
    std::string file;
    std::string header;
    std::string reason;
};

}  // namespace knot::core
