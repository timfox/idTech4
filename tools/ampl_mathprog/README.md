# AMPL / MathProg → runtime game tuning (id Tech 5–style)

id Tech **5 / 6** used **AMPL-class** algebraic models in the **tools pipeline** to optimize resource budgets and tuning constants, then shipped **numeric results** into the runtime—not a full AMPL interpreter inside every frame.

This directory does the same for **single-player combat tuning** (dynamic protection + armor split):

1. **`resource_budget.mod`** — LP for **`dmg_step`** / **`rec_step`** (dynamic `g_damageScale` ramp).
2. **`armor_balance.mod`** — LP that fits **`easy`** / **`hard`** armor absorption fractions to targets while enforcing a minimum spread (replaces hard-coded `0.4` / `0.2` on respawn for low vs high single-player skill).
3. **`nightmare_drain.mod`** — small **MIP** for nightmare skill passive drain: integer **`take_amt`** (health per tick) and **`take_period`** (seconds between ticks) → **`g_optimNightmareTakeAmt`** / **`g_optimNightmareTakePeriodSec`** (defaults match stock **5** / **5**).
4. **`generate_resource_cfg.sh`** — runs **`glpsol`** on all three models and merges **`base/optim/resource_tuned.cfg`** (also sets legacy **`g_healthTakeAmt`** / **`g_healthTakeTime`** to the same integers for UI and other code paths).
5. **`Game_local::Init`** runs **`exec optim/resource_tuned.cfg`** when that file exists (after CVars register).

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
- **`g_optimArmorEasy`** / **`g_optimArmorHard`** — `g_armorProtection` after respawn on skill **0–1** vs **2+** in single-player (defaults **0.4** / **0.2**, matching stock behavior).
- **`g_optimNightmareTakeAmt`** / **`g_optimNightmareTakePeriodSec`** — nightmare (`g_skill` **3**) passive health drain; replaces direct use of **`g_healthTakeAmt`** / **`g_healthTakeTime`** in that code path (original CVars remain for other tools / configs).

Tuning the **`.mod`** files and re-running **`generate_resource_cfg.sh`** is the same workflow as shipping a new **`resource_tuned.cfg`** from an offline solve.
