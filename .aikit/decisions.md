# Decisions

Why this project is built the way it is, oldest first. One entry per decision that changes a
contract, a dependency, a data format or the architecture; IDs are consecutive and never reused.

<!-- Entry format:
### ADR-NNN: <decision in one line>
- **Date**: YYYY-MM-DD
- **Status**: Accepted
- **Context**: <what forced the decision>
- **Decision**: <what was decided>
- **Consequences**: <what follows, including what it costs>
-->

### ADR-001: Functional core, imperative shell and explicit state machines for Knot
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: User directive: a unique C++17 tool for programmers, with Functional Core / Imperative Shell, I/O at the edges and Explicit State Machines treated as hard architectural constraints; no third-party dependencies.
- **Decision**: src/core is pure C++17 with no I/O; the include lexer, iterative DFS cycle detector, identifier scanner and cost worklist are enum state machines with value-in/value-out transition functions. src/shell owns all I/O in files.cpp and a pure argv decoder in cli.cpp; main.cpp only wires parse -> read -> analyze -> write. Reports are deterministic by sorting every input and output. AIKit's ADR-315 Python compat scaffold is retained as tool infrastructure, not part of the implementation.
- **Consequences**: Every state transition is directly unit-testable without files or processes; the same tree always produces byte-identical reports so Knot can be a CI gate; the two scaffold .py files remain in the project for the compatibility gate and are documented as such in the README.

### ADR-002: Add path queries and golden-baseline regression diffing
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: Cycles and cost reports identify a problem but not the exact include chain, and nothing detects that coupling grew between revisions; the user asked for continued autonomous implementation of programmer-facing features under the same architectural constraints.
- **Decision**: Added a pure BFS state machine for shortest include paths (knot why), a pure JSON lexer/parser for Knot's own versioned report schema, and a pure snapshot/diff model (knot diff) with CI regression flags. Relative includes are resolved to scanned documents by path suffix inside analyze(), so absolute roots do not duplicate nodes. All new I/O stays in files.cpp.
- **Consequences**: Coupling regressions can be blocked in CI with --fail-on-regression, and 'why is this header here' is answerable without a compiler; a snapshot round-trip property test and 41 new checks protect the new code; the version string moved to 0.2.0. A baseline produced by Knot 0.1.x contains duplicate nodes and must be regenerated.
