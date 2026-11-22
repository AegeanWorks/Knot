#include "cli.hpp"

#include <cctype>

namespace knot::shell {
namespace {

bool parse_positive(const std::string& text, std::size_t& out) {
    if (text.empty()) return false;
    std::size_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + static_cast<std::size_t>(c - '0');
        if (value > 100000) return false;
    }
    if (value == 0) return false;
    out = value;
    return true;
}

}  // namespace

std::string usage_text() {
    return "knot - a zero-dependency C++ include-graph analyzer\n"
           "\n"
           "Usage:\n"
           "  knot scan <dir> [--format text|json] [--top N] [--fail-on-cycle]\n"
           "  knot cycles <dir> [--format text|json]\n"
           "  knot hotspots <dir> [--format text|json] [--top N]\n"
           "  knot demo\n"
           "  knot help | knot --help | knot --version\n"
           "\n"
           "Exit codes: 0 clean, 1 finding (--fail-on-cycle), 2 usage, 3 I/O.\n";
}

Command parse_args(int argc, char** argv) {
    Command cmd;
    if (argc < 2) {
        cmd.error = "missing subcommand";
        return cmd;
    }
    std::string sub = argv[1];
    if (sub == "--help" || sub == "-h" || sub == "help") {
        cmd.kind = CommandKind::Help;
        return cmd;
    }
    if (sub == "--version" || sub == "version") {
        cmd.kind = CommandKind::Version;
        return cmd;
    }
    if (sub == "demo") {
        cmd.kind = CommandKind::Demo;
        return cmd;
    }
    if (sub == "scan") cmd.kind = CommandKind::Scan;
    else if (sub == "cycles") cmd.kind = CommandKind::Cycles;
    else if (sub == "hotspots") cmd.kind = CommandKind::Hotspots;
    else {
        cmd.error = "unknown subcommand: " + sub;
        return cmd;
    }

    int index = 2;
    if (index < argc && argv[index][0] != '-') cmd.root = argv[index++];

    for (; index < argc; ++index) {
        std::string flag = argv[index];
        if (flag == "--fail-on-cycle") {
            cmd.fail_on_cycle = true;
        } else if (flag == "--format") {
            if (++index >= argc) {
                cmd.error = "--format needs text or json";
                return cmd;
            }
            cmd.format = argv[index];
            if (cmd.format != "text" && cmd.format != "json") {
                cmd.error = "unknown format: " + cmd.format;
                return cmd;
            }
        } else if (flag == "--top") {
            if (++index >= argc || !parse_positive(argv[index], cmd.top)) {
                cmd.error = "--top needs a positive integer";
                return cmd;
            }
        } else {
            cmd.error = "unknown flag: " + flag;
            return cmd;
        }
    }
    return cmd;
}

}  // namespace knot::shell
