# Experimental ECS / EDA layer (EnTT)

This repository’s **authoritative** simulation model remains **id Tech 4**:

- **Entities:** `idEntity`, spawn defs, `idGameLocal::entities[]`, networking, savegames.
- **Events:** `idEvent` / `idEventDef`, `idEvent::ServiceEvents()`, and the existing game queues.

The code under `neo/game/experimental/` adds an **optional, parallel** registry using **[EnTT](https://github.com/skypjack/entt)** so new features can follow **entity–component–system** and **event-queue** patterns **without** replacing or mutating the stock engine paths.

## Design rules

1. **No replacement:** EnTT entities are **not** `idEntity` indices. Do not assume parity with `entityNumber`, PVS, or snapshots.
2. **Map-scoped:** The registry is created at **`MapPopulate`** and cleared at **`MapClear`** / map shutdown. It does not persist across save/load unless you explicitly add that (not implemented).
3. **Opt-in tick:** Set **`g_ecsExperimental 1`** to run the lightweight experimental **`Frame`** each game frame. Default is **0** (zero overhead beyond a null check).
4. **EDA-style events:** The sample uses a simple **`idStrList` queue** drained in **`Frame`** (see `GameExperimentalECS.cpp`). This is **not** the script `idEvent` system; it is a separate pattern for new subsystems.

## Console

- **`ecsExperimentalProbe`** — prints status (`g_ecsExperimental`, live entity count).
- **`ecsExperimentalProbe event <message>`** — enqueues a string event processed on the next tick (when `g_ecsExperimental` is 1).

## Vendored dependency

Single-header EnTT lives at **`third_party/entt/single_include/entt/entt.hpp`** (MIT license in `third_party/entt/LICENSE`). The Game project adds **`$(MSBuildThisFileDirectory)..\third_party\entt\single_include`** via **`neo/_Game.props`**.

## Integration ideas (future)

- Mirror **read-only** state from selected `idEntity` instances into components each frame.
- Drive **tools**, **AI experiments**, or **AIML host bridges** that §9 of the AIML 3.0 spec refers to as ECS/event integration—still exposing **stable strings** to scripts rather than raw pointers.
