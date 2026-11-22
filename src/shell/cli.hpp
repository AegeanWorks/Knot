// Imperative shell's argument decoder. Pure: argv in, Command out, no I/O.
#pragma once

#include <cstddef>
#include <string>

namespace knot::shell {

enum class CommandKind { Scan, Cycles, Hotspots, Demo, Help, Version };

struct Command {
    CommandKind kind = CommandKind::Help;
    std::string root = ".";
    std::string format = "text";
    std::size_t top = 10;
    bool fail_on_cycle = false;
    std::string error;
};

Command parse_args(int argc, char** argv);
std::string usage_text();

}  // namespace knot::shell
