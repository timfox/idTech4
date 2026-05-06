# AMPL / MathProg → runtime game tuning (id Tech 5–style)

id Tech **5 / 6** used **AMPL-class** algebraic models in the **tools pipeline** to optimize resource budgets and tuning constants, then shipped **numeric results** into the runtime—not a full AMPL interpreter inside every frame.

This directory does the same for **Doom 3 BFG–class dynamic player protection**:

1. **`resource_budget.mod`** — GNU MathProg model (AMPL-compatible subset) that picks **`dmg_step`** and **`rec_step`** bounds for smoother difficulty tuning.
2. **`generate_resource_cfg.sh`** — runs **`glpsol`**, parses the solution, writes **`base/optim/resource_tuned.cfg`** with `set g_optimDmgStep …` / `set g_optimRecStep …`.
3. **`Game_local::Init`** runs **`exec optim/resource_tuned.cfg`** when that file exists (after CVars register), so the engine **loads optimized numbers automatically**.

## Prerequisites

```bash
sudo apt-get install -y glpk-utils   # provides glpsol
```

## Regenerate tuned cfg

From repo root:

```bash
make -C tools/ampl_mathprog gen
```

Or:

```bash
bash tools/ampl_mathprog/generate_resource_cfg.sh
```

## In-game CVars (runtime)

- **`g_optimDmgStep`** — per-hit damage-scale decay when dynamic protection is active (replaces hard-coded `0.05`).
- **`g_optimRecStep`** — idle recovery increment toward `1.0` (replaces hard-coded `0.05`).
- **`g_optimDmgInterval`** — ms between decay applications (default **500**, was hard-coded).
- **`g_optimRecInterval`** — ms between recovery ticks (default **500**).

Tuning the **`.mod`** and re-running **`generate_resource_cfg.sh`** is the same workflow as shipping a new **`resource_tuned.cfg`** from an offline solve.
