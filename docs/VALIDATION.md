# MATLAB/Simulink acceptance evidence

All functional acceptance was executed in local MATLAB/Simulink. No Python
simulation/test or standalone C test substitutes for the following results.

## Reproduction and environment

MATLAB 9.11.0.1769968 R2021b, Simulink 10.4, MinGW64 C 6.3.0, Windows called
from WSL. MATLAB + Simulink + compatible C MEX compiler suffice; no Simscape,
Electrical, Control System or Signal Processing Toolbox is required. Normal
simulation only; operating points/Fast Restart/code generation are disallowed.

From the repository in MATLAB:

```matlab
addpath('matlab'); run_all
```

WSL command actually used:

```sh
'/mnt/d/Program Files/MATLAB/R2021b/bin/matlab.exe' \
  -sd 'D:\AI_CCM_BOOST_PFC' -batch "addpath('matlab'); run_all"
```

The build uses `mex -R2017b`, `-std=c99 -Wall -Wextra`, the installed Simulink
headers, `dsp_sfun.c`, `dsp_behavior.c`, `pfc_control.c`. It selects the local
MinGW installation when present. Other machines should first configure their
own supported compiler with `mex -setup C`. Run individual `validate_m1`,
`validate_m2` or `validate_m3` for staged acceptance.

Logs: `evidence/M*/matlab.txt`. Models and build/run/assertion scripts are in
`matlab/`. Scenario CSVs and PNGs are tracked. Full long-run event CSVs are
compressed as `.csv.gz` (MATLAB `gunzip` restores them). Small excerpts retain
startup, a representative steady switching window, and Trip events. Raw MAT
traces and long uncompressed CSVs remain local and are reproducibly regenerated;
they are deliberately ignored by Git. `summary.json`, `calibration.csv` and
`efficiency.csv` provide machine-readable results. `make test` / `make all`
are auxiliary C99 compile checks only.

## M1 [VERIFIED]

`validate_m1` passed at 100MHz TBCLK / TBPRD=1514 / 1515 ticks per period:
66006.600660066Hz, +0.010001% from 66kHz. Tests assert ZERO/SOCA/PRD/CMPA,
CMPA=0 and CMPA=PRD, actual PWM edges and TBCTR=1514 at PRD, Shadow writes
10ns before / exactly at / 10ns after ZERO. Same-time writes miss that load.

Trip at 1234.567ns is not TBCLK-aligned and forces gate low at that event.
Release alone stays latched. Explicit eligible clear restores at ZERO;
HIGH held clear cannot silently rearm after a new fault; a clear request
without safe enable is rejected even if enable later returns. Simultaneous
ZERO/Trip produces no actual PWM high pulse. Timing assertion tolerance 1ps.
The short-pulse test additionally asserts that a real Trip event occurred,
preventing a vacuous latch/recovery PASS.

## M2 [VERIFIED]

`validate_m2` passed: ACQ=80ns, conversion=250ns, N=1, IRQ latency=120ns,
ISR execution=800ns. IL/VBUS/VIN hold at 80/410/740ns, EOC at 330/660/990ns,
IRQ at 990ns, entry/control at 1110ns, Shadow write/exit at 1910ns; new CMPA
loads at 15150ns. A 0.8V hold survives a change to 2.8V before EOC.

N=4 uses independent windows ending 80/410/740/1070ns. IL codes
[124,372,621,869] average to 497 at 1320ns. Negative/zero/half-scale/Vref/
overrange tests pass on all channels. ISR writes at 15140/15150/15160ns
exercise the ZERO race and actual compare edge. Deliberately busy ADC drops
SOCA and reports three overruns. A 50us ISR fills one waiting IRQ slot,
reports subsequent loss, and later consumes the preserved 0.4V snapshot
rather than the newer 0.8V result. Trip at 31333.333ns still shuts down while
ISR runs and a request waits. Every hop has a timestamp CSV.

## M3 analog chain [VERIFIED]

`validate_analog` uses the same Simulink subsystem as the power-stage loop.
Both voltage channels are checked at 0 / 311.126984 / 400 / 480V. Expected
ADC codes: 0 / 2655 / 3413 / 4095. At exactly 480V, the ADC pin is 3.3V and
full-scale clipping decodes to 479.8828125V, consistent with 480/4096 V/count.

Current tests at 0 / 1 / 15 / 21.213203A produce 0 / .05 / .75 / 1.060660V
and codes 0 / 62 / 931 / 1317. Current resolution is .01611328125A/count.
The first-order amplifier step response measures tau=318.358072ns,
fc=499924.322Hz (nominal tau=318.309886ns, 500kHz), within 0.2% acceptance.

The analog ramp test with +1mV comparator error crosses the raw threshold at
969.504793889ns; 3.3V OC output flips at 999.504793889ns. Measured propagation
is 30.000000000ns (tolerance .1ns). Trip and actual PWM-low occur at the same
OC event within 1ps. Analog blocks, not C-side current comparison, generate it.

## M3 complete switched loop [VERIFIED]

Explicit ideal switched Boost equations are integrated in Simulink; the gate
comes from the C99 peripheral block and the AVG controller runs only at ISR
entry on the saved ADC image. Bus starts precharged at 400V. Source is
220Vrms/50Hz; L=220uH, C=1200uF; nominal resistor load is 3300W at 400V.

| Measurement | Nominal stable window .46-.50s | After 3300->2475W load step, .96-1.00s |
|---|---:|---:|
| Mean bus V | 400.8015 | 400.8699 |
| Bus ripple Vpp | 20.9274 | 15.4536 |
| Input RMS A (switching ripple included) | 15.1706 | 11.4434 |
| Measured input W | 3311.7696 | 2482.9405 |
| Measured load W | 3314.3459 | 2486.2237 |
| Total PF | .992283 | .986250 |
| THD, harmonics 2..40, PWM-cycle averaged current | 5.2193% | 7.1939% |
| Peak IL A in measured window | 22.7345 | 17.3975 |
| Maximum measured switching-cycle ripple App | 7.1168 | 7.0587 |
| ADC / IRQ overruns | 0 / 0 | 0 / 0 |

Means/RMS/PF are time-weighted integrals, never unweighted means of variable
solver samples. THD uses signed line current reconstructed from rectified iL,
per-PWM-cycle integral averaging and least-squares 50Hz harmonic fitting. It
excludes switching-carrier distortion; PF includes raw switching ripple.
The small Pin/Pout difference includes changing stored energy and solver
error over the selected transient window; it is not modeled efficiency loss.
No PF/THD threshold was invented or claimed before measurement.

The load is resistive: conductance changes at .5s, so 2475W is its nominal
400V rating, explaining measured power above that at a 400.87V mean bus.
The `.png` waveform records the full transient. A preliminary .25s-post-step
window still averaged 406.738V and failed the +/-5V criterion; the experiment
was extended to observe actual recovery, without changing the criterion or
claiming a .25s recovery time. The final .5s-post-step window passes.
`measure_load_response` additionally measures the saved waveform using complete
20ms line-period means: peak envelope 425.0481V, recovery to a persistent
400+/-5V band in .28s, resolution .02s. See `load_step_response.json` and
`load_step_envelope.csv`. This is a measured slow transient, not a promised
regulation specification. The final recovery helper was also executed directly
on the saved MATLAB trace; no alternate simulator was used.

CCM check: cycle minima >.01A for 100% of measured cycles with
`abs(sin(line_phase))>.05` in both windows. Exact/near line zeros are explicitly
excluded; global all-phase CCM is not claimed. Ideal nominal prediction
`Delta_i=Vin*(1-Vin/400)*Tpwm/L` peaks at 6.886App near Vin=200V; simulated
ripple reflects the actual varying bus. Ideal small-ripple bus estimate is
21.88Vpp. These predictions are INFERRED, distinct from measured numbers.

Full-plant fault test sets the teaching comparator threshold to .7V to force
an event from actual inductor current. At 1.328410936ms OC HIGH latches Trip;
gate remains low after OC returns low. Default threshold remains 2V in the
nominal case. Fault reset test separately trips during ISR: computed CMPA=537
at 1110ns is discarded after Trip at 1500.123ns; exit at 1910ns writes zero.
An eligible rearm loads zero at 15150ns; the old command cannot reappear.

## Efficiency [VERIFIED]

| Case | Physical time | Event rows | Solver output time points | Wall time, representative final run |
|---|---:|---:|---:|---:|
| Isolated analytic PWM | 60us | 25 | 25 | .069s |
| Isolated 10ns fixed-step reference | 60us | n/a | 6001 | .050s |
| Full nominal switched PFC | .5s | 792092 | 1350973 | 13.018s |
| Full load-step PFC | 1s | 1584164 | 2700302 | 25.603s |

The tiny fixed-step reference is simpler and faster at this short duration;
no wall-time speedup is claimed. Both references assert identical PWM edge
times. The full .5s loop would require 50,000,000 10ns steps, versus 1,350,973
actual output times (Refine=1), roughly 41 time points per 1515-tick period.
Mean step .370us, maximum 5us; real acquisition, conversion, switch, analog
and Trip events still receive solver visits. The full plant was never run at
10ns fixed step. Timed model events are analytically scheduled, not counted
one TBCLK at a time. Wall times include model compilation and synchronous
logging but exclude post-run plotting/gzip; see latest logs/JSON for exact
machine-dependent values.

## Failures, corrections and bounded reviews

1. MEX selected-compiler metadata was present but the batch process did not
   discover MinGW. Explicit MW_MINGW64_LOC and mex -setup C fixed discovery.
2. Timer-only ZC reached ZERO a floating rounding amount early and lost the
   simultaneous Trip race. Exact variable sample hits plus no-early-consume
   semantics fixed it. Next-hit callback runs before Update, requiring local
   prediction, as demonstrated by the initial next-hit=0 error.
3. TransferFcn internal state scaling amplified absolute solver tolerance;
   the 1A calibration failed. State-space amplifier state now has volts as
   its unit; bandwidth and all pin/code calibration assertions pass.
4. Auto adaptive ZC threshold could ignore the load-step time root. Set an
   explicit 1e-12 signal threshold instead of suppressing the diagnostic.
5. Review 1, timer/diagnostic/source boundaries: use canonical accepted time
   for TBCTR at PRD. A one-microsecond test Trip could be missed when the
   From Workspace table did not advertise a discontinuity; explicit duplicate
   timestamps with interpolation/source ZC fixed this. Test asserts the Trip
   itself as well as the subsequent latch. R2021b SimulationInput requires
   one block parameter per setBlockParameter call, now respected.
6. Review 2, fault/state handoff: reset staged duty/integrators/Shadow on Trip
   and block stale ISR publication. MATLAB control_reset verifies the full
   old-command/Trip/exit/rearm sequence. No extra broad review cycles added.

Reproduce the documented slow-transient failure without changing production
parameters:

```matlab
addpath('matlab'); build_mex; create_pfc_model;
p=pfc_parameters; p.duration=.5; p.load_step_time=.25;
o=pfc_case('pfc_short_step',p,'ccm_boost_pfc');
m=pfc_metrics(o,p,.46,.50);  % expected bus mean about 406.74 V, outside +/-5V
```

## Limits and evidence status

[VERIFIED] The assertions, models, plots and CSV timestamps described above
ran in MATLAB/Simulink; the final `run_all` reports ALL STAGES PASS.
[INFERRED] Formula predictions and comparisons to the ideal teaching plant.
[UNKNOWN] Any real F280039C TRM/register/ADC aperture/CPU timing equivalence,
physical comparator limits, thermal behavior, EMI, real load/input range or
hardware protection readiness. No hardware was modified or energized.

Only single up-count PWM/OST, single sequential ADC and nonpreemptive ISR are
modeled. No dead band, complementary gates, real register/BSP layer, comparator
hysteresis/noise distribution, semiconductor losses, shunt dissipation or
online RMS estimator. The 50mOhm is a sense ratio, not a heat-producing element
in the plant. Trip gates off the controlled switch; a Boost input diode can
still conduct when input voltage exceeds bus voltage. Narrow unadvertised
external pulses that cross and recross entirely between solver trials are a
general ZC limitation; scheduled stimuli must advertise their breakpoints.
See the exact fault-chain evidence in FAULT_CHAINS.md.
