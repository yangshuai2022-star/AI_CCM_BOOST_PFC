# Local audit before implementation (Issue #1)

Baseline: clean main `7afeacc`, three commits. Public reusable source: this
repository's C99 event scheduler, behavior contract, C tests and MATLAB plotter.
Callbacks and the internal analog comparator conflict with the Issue; replace
these interfaces while preserving the event scheduler, ZERO/Shadow semantics.

Actually probed: MATLAB 9.11.0.1769968 R2021b, Simulink 10.4, selected MinGW64
6.3.0 C compiler at `D:\MULT_PFC_DEFINE\toolchains\mingw64`.
An unrelated MATLAB exit hook reports unrecognized `Settings`; acceptance
requires assertions and explicit PASS, not merely a zero process exit code.

Inspected existing assets (none imported to this public repository):

| Local location | Assets / interface | Decision |
|---|---|---|
| `workspace_v12/EV_Charger_station_2026/tools/MATLAB/PFC/peripheral_sim` | Continuous C MEX, DWork, timer ZC, major preview/update commit; Vienna power circuit | GitHub metadata confirms private. No source, register map, parameters or circuit copied. Topology also differs. |
| `workspace_v12/MULT_PFC_V0/tools/sim_fixpoint` | ADC/controller/feedback split, PWM count S-Function, Boost Simscape `.slx` | Fixed tick and calloc conflict with task. No source or parameters copied. |
| `/home/yangs/pfc_simulink_stage`, `pfc_simscape_stage`, `pfc_sil_stage` | Boost equations and model builders, fixed scheduler | Private/local provenance; no import. |
| `D:\MULT_PFC_DEFINE` | MEX examples and multimode control scripts | Reuse installed compiler only. |

Run from repository in MATLAB: `addpath('matlab'); validate_m1`.
Gaps: M1 event S-Function, M2 ADC/ISR, M3 separate teaching power circuit and
AVG loop. No firmware project modifications or hardware equivalence claim.
