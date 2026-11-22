// Imperative shell's argument decoder. Pure: argv in, Command out, no I/O.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace knot::shell {

enum class CommandKind { Scan, Cycles, Hotspots, Stats, Csv, Why, Diff, Impact, Graph, Check, Init, Baseline, Report, Demo, Help, Version };

struct Command {
    CommandKind kind = CommandKind::Help;
    std::string root = ".";
    std::vector<std::string> roots;
    std::string target;
    std::string from;
    std::string baseline;
    std::string output;
    std::string ignore_file;
    std::string policy_file;
    std::string csv_kind = "costs";
    std::string format = "text";
    std::size_t top = 10;
    std::size_t fail_cost = 0;
    std::size_t fail_hotspot = 0;
    bool fail_on_cycle = false;
    bool fail_on_regression = false;
    bool force = false;
    std::string error;
};

Command parse_args(int argc, char** argv);
std::string usage_text();

}  // namespace knot::shell
