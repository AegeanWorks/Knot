// Pure graph analysis. Stateful traversals are explicit state machines.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "types.hpp"

namespace knot::core {

// ---- Include graph -------------------------------------------------------

IncludeGraph build_graph(const std::vector<IncludeEdge>& edges);

// ---- Cycle detection: explicit iterative DFS state machine ---------------

enum class Mark { White, Gray, Black };

struct DfsFrame {
    std::size_t node = 0;
    std::size_t next = 0;
};

struct DfsState {
    std::vector<Mark> marks;
    std::vector<DfsFrame> stack;
    std::vector<std::string> path;
    std::vector<Cycle> cycles;
    std::size_t next_root = 0;
    bool done = false;
    bool found_this_step = false;
};

DfsState dfs_init(const IncludeGraph& g);
// One pure micro-step. Emits at most one cycle into `state.cycles`.
DfsState dfs_step(DfsState state, const IncludeGraph& g);
std::vector<Cycle> find_cycles(const IncludeGraph& g);

// ---- Cost and fan-in -----------------------------------------------------

std::vector<Cost> transitive_costs(const IncludeGraph& g);
std::vector<Hotspot> fan_in(const IncludeGraph& g);

// ---- Forward-declaration heuristic: explicit token state machine ---------

enum class TokenState {
    Seeking,
    Identifier,
    AfterIdentifier,
    LineComment,
    BlockComment,
    BlockStar,
    StringLit,
    StringEscape,
    CharLit,
    CharEscape,
    SlashPending,
};

struct IdentState {
    TokenState state = TokenState::Seeking;
    std::string current;
};

struct Token {
    std::string text;
    char next_significant = '\0';
};

struct ScanStep {
    IdentState state;
    bool emitted = false;
    Token token;
};

ScanStep ident_step(IdentState state, char c);
std::vector<Token> scan_identifiers(const std::string& text);
bool can_forward_declare(const std::string& text, const std::string& type_name);
std::string type_name_from_header(const std::string& header);

// ---- Recommendations -----------------------------------------------------

std::vector<Recommendation> recommendations(
    const IncludeGraph& g,
    const std::vector<IncludeEdge>& edges,
    const std::vector<SourceDoc>& docs,
    const std::vector<Cycle>& cycles);

}  // namespace knot::core
