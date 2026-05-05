# Goal-Oriented Action Planning (GOAP) for `idAI`

Optional **planner** that runs **after** the normal script `UpdateScript()` path each think, so existing Doom 3 monster scripts keep working; GOAP only issues **native move / melee** calls when enabled.

## Enabling on an entity

Spawn keys on an **`idAI`** (monster) entityDef or map entity:

| Key | Default | Meaning |
|-----|---------|---------|
| `goap` | `0` | Set **`1`** to enable GOAP for this AI |
| `goap_goal` | `15` | Bitmask of desired world fluents (bits 0–3: has enemy, enemy visible, enemy reachable, in melee range) |
| `goap_replan_ms` | `500` | Minimum milliseconds between full replans |
| `goap_meleeDef` | *(empty)* | **`damage`** def name passed to `AttackMelee` for the `goap_melee` action (required for melee step to be applicable) |

## CVars

- **`g_goap`** — master switch (default `1`); when `0`, GOAP does nothing even if `goap` is set on the entity.
- **`g_goapDebug`** — print plan length / failure to the console.
- **`g_goapMaxNodes`** — A* expansion cap per replan (default `512`).

## Built-in actions

The first implementation ships three operators:

1. **`goap_move_to_enemy`** — `MoveToEnemy()`, waits for `AI_MOVE_DONE`.
2. **`goap_face_enemy`** — `FaceEnemy()`, waits for `AI_MOVE_DONE`.
3. **`goap_melee`** — `AttackMelee(goap_meleeDef)` when `TestMelee()` is true (no move wait).

Planning uses a **bitmask world state** built from existing `AI_*` script booleans plus `TestMelee()`, and **optimistic simulation** (each action is assumed to achieve its postconditions in one step for search purposes). The executor still applies real `idAI` methods each frame.

## Save games

GOAP runtime state is **not** serialized in save files (to preserve compatibility with older saves). After load, `GoapInitFromSpawn()` runs from `idAI::Restore` so parameters come from **`spawnArgs`** again; the plan is rebuilt on the next replan tick.
