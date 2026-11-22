#include "cli.hpp"

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
           "                  [--fail-on-cost N] [--fail-on-hotspot N]\n"
           "                  [--ignore <file>]\n"
           "  knot cycles <dir> [--format text|json]\n"
           "  knot hotspots <dir> [--format text|json] [--top N]\n"
           "  knot stats <dir> [--format text|json]\n"
           "  knot csv <dir> [--kind costs|hotspots|recommendations]\n"
           "  knot why <dir> <header> [--from <file>] [--format text|json]\n"
           "  knot impact <dir> <header> [--format text|json]\n"
           "  knot graph <dir> [--format dot|mermaid]\n"
           "  knot check <dir> [--policy <file>] [--format text|json]\n"
           "  knot init <dir> [--force]\n"
           "  knot baseline <dir> <file> [--force]\n"
           "  knot report <dir> <file> [--force]\n"
           "  knot diff --baseline <file> --current <dir> [--format text|json]\n"
           "                  [--fail-on-regression]\n"
           "  knot demo\n"
           "  knot help | knot --help | knot --version\n"
           "\n"
           "  --root <dir>          repeatable: scan several trees in one graph\n"
           "\n"
           "Exit codes: 0 clean, 1 finding/regression, 2 usage, 3 I/O.\n";
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
    else if (sub == "stats") cmd.kind = CommandKind::Stats;
    else if (sub == "csv") cmd.kind = CommandKind::Csv;
    else if (sub == "why") cmd.kind = CommandKind::Why;
    else if (sub == "diff") cmd.kind = CommandKind::Diff;
    else if (sub == "impact") cmd.kind = CommandKind::Impact;
    else if (sub == "graph") cmd.kind = CommandKind::Graph;
    else if (sub == "check") cmd.kind = CommandKind::Check;
    else if (sub == "init") cmd.kind = CommandKind::Init;
    else if (sub == "baseline") cmd.kind = CommandKind::Baseline;
    else if (sub == "report") cmd.kind = CommandKind::Report;
    else {
        cmd.error = "unknown subcommand: " + sub;
        return cmd;
    }

    int index = 2;
    bool current_seen = false;
    if (cmd.kind != CommandKind::Diff && index < argc && argv[index][0] != '-') {
        cmd.root = argv[index++];
    }
    if (cmd.kind == CommandKind::Baseline || cmd.kind == CommandKind::Report) {
        if (index >= argc) {
            cmd.error = sub + " needs an output file";
            return cmd;
        }
        if (cmd.kind == CommandKind::Baseline) cmd.baseline = argv[index++];
        else cmd.output = argv[index++];
    }
    if (cmd.kind == CommandKind::Why || cmd.kind == CommandKind::Impact) {
        if (index >= argc) {
            cmd.error = sub + " needs a target header";
            return cmd;
        }
        cmd.target = argv[index++];
    }
    if (cmd.kind == CommandKind::Graph) cmd.format = "dot";

    for (; index < argc; ++index) {
        std::string flag = argv[index];
        if (flag == "--fail-on-cycle") {
            cmd.fail_on_cycle = true;
        } else if (flag == "--fail-on-regression") {
            cmd.fail_on_regression = true;
        } else if (flag == "--force") {
            cmd.force = true;
        } else if (flag == "--format" || flag == "--top" || flag == "--from" ||
                   flag == "--baseline" || flag == "--current" || flag == "--ignore" ||
                   flag == "--policy" || flag == "--root" || flag == "--kind" ||
                   flag == "--fail-on-cost" || flag == "--fail-on-hotspot") {
            if (++index >= argc) {
                cmd.error = flag + " needs a value";
                return cmd;
            }
            std::string value = argv[index];
            if (flag == "--format") {
                cmd.format = value;
                bool allowed = cmd.format == "text" || cmd.format == "json" ||
                               cmd.format == "dot" || cmd.format == "mermaid";
                if (!allowed) {
                    cmd.error = "unknown format: " + cmd.format;
                    return cmd;
                }
                if (cmd.kind == CommandKind::Graph && cmd.format != "dot" &&
                    cmd.format != "mermaid") {
                    cmd.error = "graph needs --format dot or mermaid";
                    return cmd;
                }
                if (cmd.kind == CommandKind::Impact && cmd.format != "text" &&
                    cmd.format != "json") {
                    cmd.error = "impact needs --format text or json";
                    return cmd;
                }
            } else if (flag == "--top") {
                if (!parse_positive(value, cmd.top)) {
                    cmd.error = "--top needs a positive integer";
                    return cmd;
                }
            } else if (flag == "--from") {
                cmd.from = value;
            } else if (flag == "--baseline") {
                cmd.baseline = value;
            } else if (flag == "--ignore") {
                cmd.ignore_file = value;
            } else if (flag == "--policy") {
                cmd.policy_file = value;
            } else if (flag == "--kind") {
                cmd.csv_kind = value;
                if (cmd.csv_kind != "costs" && cmd.csv_kind != "hotspots" &&
                    cmd.csv_kind != "recommendations") {
                    cmd.error = "unknown csv kind: " + cmd.csv_kind;
                    return cmd;
                }
            } else if (flag == "--root") {
                cmd.roots.push_back(value);
            } else if (flag == "--current") {
                cmd.root = value;
                current_seen = true;
            } else if (flag == "--fail-on-cost") {
                if (!parse_positive(value, cmd.fail_cost)) {
                    cmd.error = "--fail-on-cost needs a positive integer";
                    return cmd;
                }
            } else if (!parse_positive(value, cmd.fail_hotspot)) {
                cmd.error = "--fail-on-hotspot needs a positive integer";
                return cmd;
            }
        } else {
            cmd.error = "unknown flag: " + flag;
            return cmd;
        }
    }

    if (cmd.roots.empty()) cmd.roots.push_back(cmd.root);
    else cmd.root = cmd.roots[0];

    if (cmd.kind == CommandKind::Diff) {
        if (cmd.baseline.empty() || !current_seen) {
            cmd.error = "diff needs --baseline <file> and --current <dir>";
            return cmd;
        }
    }
    return cmd;
}

}  // namespace knot::shell
