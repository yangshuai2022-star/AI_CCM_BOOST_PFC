# MATLAB/Simulink validation

Environment: MATLAB R2021b 9.11.0.1769968, Simulink 10.4, MinGW64 C 6.3.0.
Only MATLAB + Simulink are required for event tests. Normal simulation,
variable-step ode45, zero crossings enabled, no Fast Restart or code generation.

Run `addpath('matlab'); validate_m1` from this repository. Script builds the
C MEX, generates and saves the `.slx`, runs assertions, saves timestamp CSVs,
MAT trace and PNG. Logs are in `evidence/M1/matlab.txt`. Native `make test`
is auxiliary only. There are no Python acceptance tests.

M1 acceptance includes 66kHz integer count semantics, ZERO/SOCA/PRD/CMPA,
Shadow before/at/after ZERO, boundary CMPA=0/PRD, fractional-ns asynchronous
Trip, persistent latch, explicit clear and simultaneous ZERO/Trip priority.
Timing tolerance: 1ps. Efficiency compares solver time points against the
6000 ticks needed by a 60us / 10ns traversal.

Initial failure: selected compiler metadata existed but MEX did not discover
MinGW in a new batch session. Build now explicitly sets MW_MINGW64_LOC and
runs mex -setup C. No failed attempt is classified as verified.

M1 [VERIFIED]: MATLAB/Simulink run passed all assertions. 60us isolated PWM:
25 committed event rows and 25 solver time points versus 6000 10ns ticks;
0.666s wall time in the initial passing run (includes model compilation).
See CSV and PNG files in evidence/M1. Actual PWM output is also asserted.

Failure found and fixed in MATLAB: a timer ZC located ZERO slightly early,
so a simultaneous input Trip briefly lost priority. Timers now use exact
variable sample hits, asynchronous Trip uses ZC, and early roots cannot
consume a timer. Simultaneous ZERO/Trip has no actual PWM high pulse.
The next-hit callback is called before Update, verified by the initial
`next hit 0 must be greater than 0` failure and fixed with local prediction.
MEX and final simulation have zero compiler/model warnings.

M2 and M3 remain pending until MATLAB acceptance passes at each stage.
Real MCU timing, polarity, register equivalence and physical hardware remain
UNKNOWN. This repository is a virtual teaching experiment only.
