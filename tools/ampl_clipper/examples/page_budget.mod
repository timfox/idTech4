/* Minimal MathProg model (AMPL-compatible subset) for offline tooling demos.
   Solve with: glpsol -m page_budget.mod
   or commercial AMPL with a compatible driver. */

var x1 >= 0;
var x2 >= 0;

minimize Cost: 3 * x1 + 2 * x2;

s.t. Demand: x1 + x2 >= 4;
s.t. Storage: 2 * x1 + x2 <= 10;

end;
