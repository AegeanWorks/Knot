#include "json.hpp"

#include <cstddef>

namespace knot::core {
namespace {

enum class LexState { Start, InString, StringEscape, InUnicode, InNumber, InKeyword, Error };

struct LexStateValue {
    LexState state = LexState::Start;
    std::string buffer;
    std::size_t hex_left = 0;
    unsigned code = 0;
};

enum class TokenKind {
    LBrace, RBrace, LBrack, RBrack, Colon, Comma, String, Number, Keyword
};

struct Token {
    TokenKind kind = TokenKind::String;
    std::string text;
};

struct LexStep {
    LexStateValue state;
    bool emitted = false;
    Token token;
    bool reprocess = false;  // the current character must be offered again
};

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

LexStep emit(LexStateValue s, TokenKind kind, const std::string& text) {
    s.state = LexState::Start;
    s.buffer.clear();
    return LexStep{s, true, Token{kind, text}, false};
}

LexStep punct(LexStateValue s, char c) {
    switch (c) {
        case '{': return emit(s, TokenKind::LBrace, "{");
        case '}': return emit(s, TokenKind::RBrace, "}");
        case '[': return emit(s, TokenKind::LBrack, "[");
        case ']': return emit(s, TokenKind::RBrack, "]");
        case ':': return emit(s, TokenKind::Colon, ":");
        case ',': return emit(s, TokenKind::Comma, ",");
        default: break;
    }
    s.state = LexState::Error;
    return LexStep{s, false, Token{}, false};
}

LexStep json_lex_step(LexStateValue s, char c) {
    switch (s.state) {
        case LexState::Start:
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') return LexStep{s, false, Token{}, false};
            if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',') return punct(s, c);
            if (c == '"') {
                s.state = LexState::InString;
                s.buffer.clear();
                return LexStep{s, false, Token{}, false};
            }
            if (is_digit(c) || c == '-') {
                s.state = LexState::InNumber;
                s.buffer.clear();
                s.buffer.push_back(c);
                return LexStep{s, false, Token{}, false};
            }
            if (is_alpha(c)) {
                s.state = LexState::InKeyword;
                s.buffer.clear();
                s.buffer.push_back(c);
                return LexStep{s, false, Token{}, false};
            }
            s.state = LexState::Error;
            return LexStep{s, false, Token{}, false};

        case LexState::InString:
            if (c == '"') return emit(s, TokenKind::String, s.buffer);
            if (c == '\\') {
                s.state = LexState::StringEscape;
                return LexStep{s, false, Token{}, false};
            }
            s.buffer.push_back(c);
            return LexStep{s, false, Token{}, false};

        case LexState::StringEscape:
            switch (c) {
                case '"': s.buffer.push_back('"'); s.state = LexState::InString; break;
                case '\\': s.buffer.push_back('\\'); s.state = LexState::InString; break;
                case '/': s.buffer.push_back('/'); s.state = LexState::InString; break;
                case 'b': s.buffer.push_back('\b'); s.state = LexState::InString; break;
                case 'f': s.buffer.push_back('\f'); s.state = LexState::InString; break;
                case 'n': s.buffer.push_back('\n'); s.state = LexState::InString; break;
                case 'r': s.buffer.push_back('\r'); s.state = LexState::InString; break;
                case 't': s.buffer.push_back('\t'); s.state = LexState::InString; break;
                case 'u':
                    s.state = LexState::InUnicode;
                    s.hex_left = 4;
                    s.code = 0;
                    break;
                default: s.state = LexState::Error; break;
            }
            return LexStep{s, false, Token{}, false};

        case LexState::InUnicode: {
            int value = hex_value(c);
            if (value < 0) {
                s.state = LexState::Error;
                return LexStep{s, false, Token{}, false};
            }
            s.code = s.code * 16 + static_cast<unsigned>(value);
            --s.hex_left;
            if (s.hex_left == 0) {
                s.buffer.push_back(s.code < 128 ? static_cast<char>(s.code) : '?');
                s.state = LexState::InString;
            }
            return LexStep{s, false, Token{}, false};
        }

        case LexState::InNumber: {
            if (is_digit(c) || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
                s.buffer.push_back(c);
                return LexStep{s, false, Token{}, false};
            }
            LexStep number = emit(s, TokenKind::Number, s.buffer);
            return LexStep{number.state, true, number.token, true};
        }

        case LexState::InKeyword: {
            if (is_alpha(c)) {
                s.buffer.push_back(c);
                return LexStep{s, false, Token{}, false};
            }
            LexStep keyword = emit(s, TokenKind::Keyword, s.buffer);
            return LexStep{keyword.state, true, keyword.token, true};
        }

        case LexState::Error:
            return LexStep{s, false, Token{}, false};
    }
    return LexStep{s, false, Token{}, false};
}

bool lex_all(const std::string& input, std::vector<Token>& tokens, std::string& error) {
    LexStateValue state;
    for (char c : input) {
        bool consumed = false;
        while (!consumed) {
            LexStep step = json_lex_step(state, c);
            state = step.state;
            if (step.emitted) tokens.push_back(step.token);
            if (state.state == LexState::Error) {
                error = "invalid character in JSON";
                return false;
            }
            consumed = !step.reprocess;
        }
    }
    if (state.state == LexState::InString || state.state == LexState::StringEscape ||
        state.state == LexState::InUnicode) {
        error = "unterminated JSON string";
        return false;
    }
    if (state.state == LexState::InNumber) {
        tokens.push_back(Token{TokenKind::Number, state.buffer});
    } else if (state.state == LexState::InKeyword) {
        tokens.push_back(Token{TokenKind::Keyword, state.buffer});
    }
    return true;
}

bool parse_number(const std::string& text, long long& out) {
    if (text.empty()) return false;
    std::size_t i = 0;
    bool negative = false;
    if (text[i] == '-') {
        negative = true;
        ++i;
    }
    if (i >= text.size()) return false;
    long long value = 0;
    for (; i < text.size(); ++i) {
        if (!is_digit(text[i])) return false;
        value = value * 10 + (text[i] - '0');
        if (value > 1000000000000LL) return false;
    }
    out = negative ? -value : value;
    return true;
}

bool parse_value(const std::vector<Token>& tokens, std::size_t& pos, int depth,
                 JsonValue& out, std::string& error) {
    if (depth > 64) {
        error = "JSON nesting too deep";
        return false;
    }
    if (pos >= tokens.size()) {
        error = "unexpected end of JSON";
        return false;
    }
    const Token& token = tokens[pos];
    switch (token.kind) {
        case TokenKind::String:
            out.type = JsonType::String;
            out.text = token.text;
            ++pos;
            return true;
        case TokenKind::Number:
            out.type = JsonType::Number;
            if (!parse_number(token.text, out.number)) {
                error = "invalid number: " + token.text;
                return false;
            }
            ++pos;
            return true;
        case TokenKind::Keyword:
            if (token.text == "true") {
                out.type = JsonType::Bool;
                out.boolean = true;
            } else if (token.text == "false") {
                out.type = JsonType::Bool;
                out.boolean = false;
            } else if (token.text == "null") {
                out.type = JsonType::Null;
            } else {
                error = "unknown keyword: " + token.text;
                return false;
            }
            ++pos;
            return true;
        case TokenKind::LBrack: {
            out.type = JsonType::Array;
            ++pos;
            if (pos < tokens.size() && tokens[pos].kind == TokenKind::RBrack) {
                ++pos;
                return true;
            }
            while (true) {
                JsonValue item;
                if (!parse_value(tokens, pos, depth + 1, item, error)) return false;
                out.array.push_back(item);
                if (pos >= tokens.size()) {
                    error = "unterminated JSON array";
                    return false;
                }
                if (tokens[pos].kind == TokenKind::RBrack) {
                    ++pos;
                    return true;
                }
                if (tokens[pos].kind != TokenKind::Comma) {
                    error = "expected ',' in JSON array";
                    return false;
                }
                ++pos;
            }
        }
        case TokenKind::LBrace: {
            out.type = JsonType::Object;
            ++pos;
            if (pos < tokens.size() && tokens[pos].kind == TokenKind::RBrace) {
                ++pos;
                return true;
            }
            while (true) {
                if (pos >= tokens.size() || tokens[pos].kind != TokenKind::String) {
                    error = "expected string key in JSON object";
                    return false;
                }
                std::string key = tokens[pos].text;
                ++pos;
                if (pos >= tokens.size() || tokens[pos].kind != TokenKind::Colon) {
                    error = "expected ':' in JSON object";
                    return false;
                }
                ++pos;
                JsonValue item;
                if (!parse_value(tokens, pos, depth + 1, item, error)) return false;
                out.object.push_back(std::make_pair(key, item));
                if (pos >= tokens.size()) {
                    error = "unterminated JSON object";
                    return false;
                }
                if (tokens[pos].kind == TokenKind::RBrace) {
                    ++pos;
                    return true;
                }
                if (tokens[pos].kind != TokenKind::Comma) {
                    error = "expected ',' in JSON object";
                    return false;
                }
                ++pos;
            }
        }
        default:
            error = "unexpected token in JSON";
            return false;
    }
}

}  // namespace

JsonResult parse_json(const std::string& input) {
    JsonResult result;
    std::vector<Token> tokens;
    if (!lex_all(input, tokens, result.error)) return result;
    if (tokens.empty()) {
        result.error = "empty JSON";
        return result;
    }
    std::size_t pos = 0;
    if (!parse_value(tokens, pos, 0, result.value, result.error)) return result;
    if (pos != tokens.size()) {
        result.error = "trailing JSON tokens";
        return result;
    }
    result.ok = true;
    return result;
}

const JsonValue* json_member(const JsonValue& value, const std::string& key) {
    if (value.type != JsonType::Object) return nullptr;
    for (const auto& entry : value.object) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

std::string json_type_name(JsonType type) {
    switch (type) {
        case JsonType::Null: return "null";
        case JsonType::Bool: return "bool";
        case JsonType::Number: return "number";
        case JsonType::String: return "string";
        case JsonType::Array: return "array";
        case JsonType::Object: return "object";
    }
    return "unknown";
}

}  // namespace knot::core
