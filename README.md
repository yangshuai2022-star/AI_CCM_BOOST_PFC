# AI_CCM_BOOST_PFC

MATLAB/Simulink teaching experiment: event-driven C99 digital peripherals,
separate AVG control and Boost power/analog circuit. Issue #1 latest design:
220Vac/50Hz -> 400V, 3.3kW, 220uH, 1200uF, 66kHz target.

In MATLAB R2021b with Simulink and a supported C MEX compiler, from repository:

```matlab
addpath('matlab'); validate_m1
validate_m2
```

The script builds MEX, creates `matlab/dsp_event_test.slx`, actually simulates
it and checks timestamp assertions. CSV, logs, plots go to `evidence/M1`.
M1 PWM/Trip and M2 ADC/ISR have passed MATLAB acceptance; stage evidence is
under `evidence/M1` and `evidence/M2`. The complete PFC power/control loop (M3)
follows after both stages pass.

[Architecture](docs/ARCHITECTURE.md), [validation](docs/VALIDATION.md),
[local asset audit](docs/LOCAL_ASSETS.md).
No Python tests substitute for MATLAB/Simulink acceptance. Native `make test`
is only an auxiliary smoke check. This model does not establish MCU hardware
or protection equivalence and is not production firmware.
