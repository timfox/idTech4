# AIML 3.0 Core reference interpreter (experimental)

This directory contains a **small C++17** implementation of the **AIML 3.0 Core Dialog** subset from [timfox/aiml-3.0-spec](https://github.com/timfox/aiml-3.0-spec) (vendored under `third_party/aiml-3.0-spec`).

It is intended as a **portable reference** for normalization, match precedence, and template evaluation—not a full production stack (no OpenUSD runtime, no extended modules, no full XML namespaces).

**USDA (ASCII Universal Scene Description)** for AIML-shaped `AimlCategory` prims is supported as a **custom interpreter** in `load_usda.cpp` (subset: `def AimlCategory`, `string pattern` / `that` / `topic` / `template`, optional `customData = { string cooldown = "..." }`, optional `custom string[] directives|requires|unless|effects` per the spec appendix). This is **not** a general USD stage engine.

## Conformance (approximate)

| Spec area | Status |
|-----------|--------|
| §5.2 Core default normalization | **Trim**, **ASCII whitespace collapse**, **ASCII case-fold** on letters `A–Z`/`a–z`. Full **Unicode NFC** and full Unicode case fold are **not** implemented (documented limitation). |
| §5.3 Match precedence | **Wildcard count** (weight `*` = 0, `_` = 1 per spec note), then **topic** (non-default beats `*`), then **`that`** present beats absent, then **document order**. |
| §6–7 | Patterns with `*` and `_`; optional **`that`**; optional **`topic`** on categories. |
| §8 Core templates | Literal text, `<random>`, `<condition>`, `<get>`, `<set>`, `<bot>`, `<map>`, `<srai>`, `<star/>`, `<think>` (silent), basic `<uppercase>` / `<lowercase>`. |
| §5.5 / §11 | **`srai_max_depth`** default 32; cycle detection for identical `(input,topic,thathash)` at same depth. |

## Build

```bash
make -C tools/aiml3
```

## Run (REPL)

```bash
./tools/aiml3/aiml3 --load pack.json
# or
./tools/aiml3/aiml3 --load bot.aiml
# or (USDA subset for AimlCategory)
./tools/aiml3/aiml3 --load test/hello.usda
```

Type lines of input; `quit` exits.

## Loaders

- **JSON:** `categories` array (or `aiml.categories`), each item: `pattern`, optional `that`, optional `topic`, `template` as string or `{ "say": "..." }`.
- **XML:** root `<aiml>`, `<category>` with `<pattern>`, optional `<that>`, optional `<template>` — **no namespace** elements only for this tool.
- **USDA:** see `test/hello.usda` and [../../docs/USD_AIML_INTERPRETER.md](../../docs/USD_AIML_INTERPRETER.md).

## Submodule

The specification text lives in:

`third_party/aiml-3.0-spec/` (git submodule → `https://github.com/timfox/aiml-3.0-spec`).
