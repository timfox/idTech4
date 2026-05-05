# USDA (ASCII USD) — custom AIML-shaped interpreter

The **`aiml3`** tool in `tools/aiml3/` can load **`.usda`** files that encode **AIML 3.0 Core** categories using the **Universal Scene Description** text syntax shown in [AIML 3.0 §E.3](https://github.com/timfox/aiml-3.0-spec) (example `def AimlCategory "vendor_coffee" (...)` with `string pattern`, `string template`, etc.).

## What this is

- A **small, self-contained lexer/parser** (`tools/aiml3/load_usda.cpp`) — **not** OpenUSD, **not** `usdview`, **not** full composition.
- Intended for **authoring / interchange** of the same logical categories as JSON/XML in the reference interpreter.

## Supported subset

- File header like `#usda 1.0` (optional).
- `def AimlCategory "name"` or `def AimlCategory name` (unquoted path segment).
- Optional metadata **`(...)`** before the body `{` (parsed and skipped).
- Body properties:
  - **`string pattern`**, **`string that`**, **`string topic`**, **`string template`** (quoted).
  - **`customData = { ... }`** with **`string cooldown = "..."`** inside (other keys ignored).
  - **`custom string[] directives`**, **`requires`**, **`unless`**, **`effects`** — parsed into `Category` (the reference `respond()` path does not enforce them yet; they are available for host extensions).

## Limitations

- **ASCII `.usda` only** (no `.usdc` / `.usdz` binary crates).
- No **references**, **payloads**, **variants**, or **composition arcs**.
- Unknown prim types or statements are skipped only heuristically; malformed files may fail with a terse error.

## Try it

```bash
make -C tools/aiml3
printf 'HELLO\nquit\n' | ./tools/aiml3/aiml3 --load tools/aiml3/test/hello.usda
```
