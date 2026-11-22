#include "lexer.hpp"

#include <cctype>

namespace knot::core {
namespace {

bool is_hspace(char c) { return c == ' ' || c == '\t' || c == '\r'; }

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Newline handling shared by every state that ends at end-of-line.
LexStep newline(LexerState s) {
    s.line += 1;
    s.at_line_start = true;
    s.state = LexState::Normal;
    s.directive.clear();
    s.path.clear();
    return LexStep{s, false, IncludeEdge{}};
}

}  // namespace

LexStep lex_step(LexerState s, char c) {
    switch (s.state) {
        case LexState::Normal:
            if (c == '\n') return newline(s);
            if (c == '#') {
                if (s.at_line_start) {
                    s.state = LexState::AfterHash;
                    s.directive.clear();
                    return LexStep{s, false, IncludeEdge{}};
                }
                s.at_line_start = false;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '/') {
                s.state = LexState::SlashPending;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '"') {
                s.state = LexState::StringLiteral;
                s.at_line_start = false;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '\'') {
                s.state = LexState::CharLiteral;
                s.at_line_start = false;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (!is_hspace(c)) s.at_line_start = false;
            return LexStep{s, false, IncludeEdge{}};

        case LexState::AfterHash:
            if (is_hspace(c)) return LexStep{s, false, IncludeEdge{}};
            if (c == '\n') return newline(s);
            s.state = LexState::Directive;
            s.directive.clear();
            s.directive.push_back(c);
            s.at_line_start = false;
            return LexStep{s, false, IncludeEdge{}};

        case LexState::Directive:
            if (is_alpha(c)) {
                s.directive.push_back(c);
                return LexStep{s, false, IncludeEdge{}};
            }
            if (is_hspace(c)) {
                if (s.directive == "include") {
                    s.state = LexState::AfterInclude;
                } else {
                    s.state = LexState::SkipLine;
                }
                s.path.clear();
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '\n') return newline(s);
            s.state = LexState::SkipLine;
            return LexStep{s, false, IncludeEdge{}};

        case LexState::AfterInclude:
            if (is_hspace(c)) return LexStep{s, false, IncludeEdge{}};
            if (c == '"') {
                s.state = LexState::InQuote;
                s.path.clear();
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '<') {
                s.state = LexState::InAngle;
                s.path.clear();
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '\n') return newline(s);
            s.state = LexState::SkipLine;
            return LexStep{s, false, IncludeEdge{}};

        case LexState::InQuote:
            if (c == '"') {
                s.state = LexState::Normal;
                s.at_line_start = false;
                return LexStep{s, true, IncludeEdge{"", s.path, s.line}};
            }
            if (c == '\n') return newline(s);
            s.path.push_back(c);
            return LexStep{s, false, IncludeEdge{}};

        case LexState::InAngle:
            if (c == '>') {
                s.state = LexState::Normal;
                s.at_line_start = false;
                return LexStep{s, true, IncludeEdge{"", s.path, s.line}};
            }
            if (c == '\n') return newline(s);
            s.path.push_back(c);
            return LexStep{s, false, IncludeEdge{}};

        case LexState::SkipLine:
            if (c == '\n') return newline(s);
            return LexStep{s, false, IncludeEdge{}};

        case LexState::LineComment:
            if (c == '\n') return newline(s);
            return LexStep{s, false, IncludeEdge{}};

        case LexState::BlockComment:
            if (c == '*') {
                s.state = LexState::StarPending;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '\n') {
                s.line += 1;
                return LexStep{s, false, IncludeEdge{}};
            }
            return LexStep{s, false, IncludeEdge{}};

        case LexState::StarPending:
            if (c == '/') {
                s.state = LexState::Normal;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '*') return LexStep{s, false, IncludeEdge{}};
            if (c == '\n') {
                s.line += 1;
                s.state = LexState::BlockComment;
                return LexStep{s, false, IncludeEdge{}};
            }
            s.state = LexState::BlockComment;
            return LexStep{s, false, IncludeEdge{}};

        case LexState::SlashPending:
            if (c == '/') {
                s.state = LexState::LineComment;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '*') {
                s.state = LexState::BlockComment;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '\n') return newline(s);
            s.state = LexState::Normal;
            s.at_line_start = false;
            return LexStep{s, false, IncludeEdge{}};

        case LexState::StringLiteral:
            if (c == '\\') {
                s.state = LexState::StringEscape;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '"') {
                s.state = LexState::Normal;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '\n') return newline(s);
            return LexStep{s, false, IncludeEdge{}};

        case LexState::StringEscape:
            if (c == '\n') {
                s.line += 1;
                s.state = LexState::StringLiteral;
                return LexStep{s, false, IncludeEdge{}};
            }
            s.state = LexState::StringLiteral;
            return LexStep{s, false, IncludeEdge{}};

        case LexState::CharLiteral:
            if (c == '\\') {
                s.state = LexState::CharEscape;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '\'') {
                s.state = LexState::Normal;
                return LexStep{s, false, IncludeEdge{}};
            }
            if (c == '\n') return newline(s);
            return LexStep{s, false, IncludeEdge{}};

        case LexState::CharEscape:
            if (c == '\n') {
                s.line += 1;
                s.state = LexState::CharLiteral;
                return LexStep{s, false, IncludeEdge{}};
            }
            s.state = LexState::CharLiteral;
            return LexStep{s, false, IncludeEdge{}};
    }
    return LexStep{s, false, IncludeEdge{}};
}

std::vector<IncludeEdge> scan_includes(const std::string& source,
                                       const std::string& file_name) {
    LexerState state;
    std::vector<IncludeEdge> out;
    for (char c : source) {
        LexStep step = lex_step(state, c);
        state = step.state;
        if (!step.emitted) continue;
        IncludeEdge edge = step.edge;
        // Trim horizontal whitespace around the directive argument.
        std::size_t first = 0;
        while (first < edge.header.size() && is_hspace(edge.header[first])) ++first;
        std::size_t last = edge.header.size();
        while (last > first && is_hspace(edge.header[last - 1])) --last;
        edge.header = edge.header.substr(first, last - first);
        if (edge.header.empty()) continue;
        edge.from = file_name;
        out.push_back(edge);
    }
    return out;
}

}  // namespace knot::core
