/* MathProg (GNU MathProg): nightmare skill (g_skill 3) passive health drain.
   Integer amounts and period (seconds) so glpsol picks a small MIP solution.
   Tune params at top; objective prefers gentler drain (lower amt, longer period)
   while keeping a minimum "pressure" so nightmare stays threatening. */

param pressure_w_amt default 6;
param pressure_w_period default 1;
param min_pressure default 38;

var take_amt integer >= 3, <= 12;
var take_period integer >= 5, <= 15;

minimize drain_cost: pressure_w_amt * take_amt + pressure_w_period * take_period;

/* Linear surrogate: higher take_amt * weight must offset short periods. */
s.t. pressure: 10 * take_amt >= min_pressure + take_period;

end;
