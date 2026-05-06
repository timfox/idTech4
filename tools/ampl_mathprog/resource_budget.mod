/* MathProg (GNU MathProg / AMPL subset) — offline resource / tuning budgets.
   Solved with: glpsol --math resource_budget.mod -o /tmp/sol.txt -w /tmp/sol.txt
   Or:        glpsol -m resource_budget.mod

   This model picks conservative step sizes for dynamic player protection
   (damage scale decay / recovery) under bounds inspired by id Tech 5–style
   offline optimization of gameplay feel vs. difficulty. */

param dmg_min default 0.15;
param dmg_max default 1.0;
param rec_min default 0.01;
param rec_max default 0.15;
var dmg_step >= dmg_min, <= dmg_max;
var rec_step >= rec_min, <= rec_max;

/* Minimize total "aggressiveness" of protection swings (smaller steps = smoother). */
minimize swing_cost: dmg_step + rec_step;

/* Require enough decay per hit window so scale can drop meaningfully before floor. */
s.t. decay_budget: dmg_step >= 0.2;

/* Recovery should not outpace decay too much (keeps dynamic protection meaningful). */
s.t. balance: rec_step <= 0.75 * dmg_step;

end;
