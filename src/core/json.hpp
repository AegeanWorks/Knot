// Minimal, pure JSON reader for Knot's versioned snapshot schema.
// The lexer is an explicit state machine; the parser is pure recursion over tokens.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace knot::core {

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolean = false;
    long long number = 0;
    std::string text;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;
};

struct JsonResult {
    bool ok = false;
    std::string error;
    JsonValue value;
};

JsonResult parse_json(const std::string& input);
const JsonValue* json_member(const JsonValue& value, const std::string& key);
std::string json_type_name(JsonType type);

}  // namespace knot::core
