// Explicit state machine that recognises `#include` directives.
// Functional core: a pure `step` folded over the characters of a source text.
#pragma once

#include <string>
#include <vector>

#include "types.hpp"

namespace knot::core {

enum class LexState {
    Normal,
    AfterHash,
    Directive,
    AfterInclude,
    InAngle,
    InQuote,
    SkipLine,
    LineComment,
    BlockComment,
    StarPending,
    SlashPending,
    StringLiteral,
    StringEscape,
    CharLiteral,
    CharEscape,
};

// Complete, immutable snapshot of the lexer's state.
struct LexerState {
    LexState state = LexState::Normal;
    std::string directive;
    std::string path;
    int line = 1;
    bool at_line_start = true;
};

// Result of one pure transition: the next state and at most one emitted edge.
struct LexStep {
    LexerState state;
    bool emitted = false;
    IncludeEdge edge;
};

// The single transition function. No I/O, no mutation of global state.
LexStep lex_step(LexerState state, char c);

// Fold `lex_step` over `source`; returns emitted edges in source order.
std::vector<IncludeEdge> scan_includes(const std::string& source,
                                       const std::string& file_name);

}  // namespace knot::core
