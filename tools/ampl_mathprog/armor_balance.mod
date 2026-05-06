/* MathProg: tune single-player armor absorption (easy vs hard skill split).
   Targets are soft; auxiliary variables linearize L1 deviation from targets. */

param target_easy default 0.40;
param target_hard default 0.20;

var easy >= 0.25, <= 0.55;
var hard >= 0.10, <= 0.35;

var pe >= 0;
var me >= 0;
var ph >= 0;
var mh >= 0;

minimize fit: pe + me + ph + mh;

s.t. dev_easy_p: pe >= easy - target_easy;
s.t. dev_easy_m: me >= target_easy - easy;
s.t. dev_hard_p: ph >= hard - target_hard;
s.t. dev_hard_m: mh >= target_hard - hard;

/* Easy skill should keep meaningfully more armor absorption than hard. */
s.t. spread: easy >= hard + 0.12;

end;
