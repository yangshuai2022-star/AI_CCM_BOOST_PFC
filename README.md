# AI_CCM_BOOST_PFC

MATLAB/Simulink teaching experiment: event-driven C99 digital peripherals,
separate AVG control and Boost power/analog circuit. Issue #1 latest design:
220Vac/50Hz -> 400V, 3.3kW, 220uH, 1200uF, 66kHz target.

In MATLAB R2021b with Simulink and a supported C MEX compiler, from repository:

```matlab
addpath('matlab'); validate_m1
```

The script builds MEX, creates `matlab/dsp_event_test.slx`, actually simulates
it and checks timestamp assertions. CSV, logs, plots go to `evidence/M1`.
M1 is the event/Trip integration stage. ADC/ISR (M2) and the complete PFC
power/control loop (M3) follow after each preceding MATLAB stage passes.

[Architecture](docs/ARCHITECTURE.md), [validation](docs/VALIDATION.md),
[local asset audit](docs/LOCAL_ASSETS.md).
No Python tests substitute for MATLAB/Simulink acceptance. Native `make test`
is only an auxiliary smoke check. This model does not establish MCU hardware
or protection equivalence and is not production firmware.
