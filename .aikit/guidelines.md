# Project Guidelines

Rules for this project only, written by people or AI agents. They add to AIKit's principles and
rank below them: a guideline never overrides a principle (PRINCIPLE-000 R6, PRINCIPLE-001 R4).
Add one with `python aikit.py guideline <name> "<rule>" --source <user|ADR-NNN|person>`, or by
hand as one line below in the form `- **G-NNN** (source): rule`; IDs are never reused.

Persisted state carries a format version. A loader refuses a version it does not support,
without modifying or quarantining that file; older versions migrate when a migration exists.

## Rules
- **G-001** (user): Implementation language is C++17 only; no other implementation language and no third-party dependencies.
- **G-002** (user): Architecture is a hard constraint: Functional Core with no I/O, Imperative Shell that owns all I/O at the edges, and all stateful behavior expressed as explicit state machines (enum + pure transition function). Verified by tests.
