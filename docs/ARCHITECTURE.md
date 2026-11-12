# Event model architecture

Three independent modules: Simulink power circuit / analog front end;
portable C99 control; portable C99 digital peripheral behavior, exposed via
`matlab/dsp_sfun.c`. No DriverLib, register maps, function pointers or explicit
heap allocation. Standard file I/O in the adapter logs committed events only.

## Timer and solver contract

Up count; 100 MHz TBCLK, 10 ns ticks. Latest Issue #1 specification:
TBPRD=1514, period=(TBPRD+1)*10 ns=15.15 us, f=66006.600660 Hz
(+0.010001% relative to requested 66 kHz). CMPA within 0..TBPRD.
ZERO sets AQ, CMPA clears AQ, PRD is a separate logged event one tick before
the next ZERO. CMPA=0 clears at ZERO without a high pulse. CMPA=TBPRD clears
at PRD before the PRD record. No strict 100% duty representation.

All times are double nanoseconds, so external fractional-ns Trip events are
preserved. Timed PWM deadlines are computed analytically on the integer
TBCLK lattice. No count loop. TBCTR is derived from elapsed phase.

Tie priority: raw Trip/disable -> eligible explicit clear request -> ZERO and
old Shadow load -> CMPA clear -> PRD -> CPU/external Shadow write.
Same-time writes miss ZERO; earlier writes load at ZERO. Trip gates actual
output independently of the AQ state. Release alone never clears the latch.
Clear is rising-edge requested only with low Trip and safe enable, committed
at the next ZERO. Holding clear high across a fault cannot rearm it.
These are teaching-model semantics; correspondence to a real MCU is UNKNOWN.

Continuous sample time plus analytically scheduled variable sample hits.
Asynchronous ZC surface: `OC_COMP_OUT-1.65V`. `mdlOutputs` previews a local state copy only at major
steps; mode vector holds gate values at minor steps. `mdlUpdate` commits
accepted state and writes logs. DWork owns each instance. A missed timer
deadline fails the simulation rather than silently sampling a future value.
Root matching tolerance is 0.1 ps; timers must never fire early. Only an exact
scheduled sample hit is mapped back to its canonical ns deadline. Next-hit
callback predicts from a local copy because MATLAB calls it before Update.

The timer ZC integration must pass MATLAB tests before use. Public API sources:
[MathWorks mdlZeroCrossings](https://www.mathworks.com/help/simulink/sfg/mdlzerocrossings.html),
[S-Function callback phases](https://www.mathworks.com/help/simulink/sfg/s-function-callback-methods.html).
The Issue permits experimentally equivalent event hits. See MathWorks'
[future-event scheduling](https://blogs.mathworks.com/simulink/2012/01/22/advanced-s-function-techniques-scheduling-future-events/).

## Ports and evidence

Input `[IL_pin_V, VBUS_pin_V, OC_logic_V, enable, clear, CMPA_ticks,
command_sequence, VIN_pin_V]`. Voltage/current physical scaling belongs to
Simulink. OC high threshold here is the digital 1.65 V logic threshold, not
an analog current-comparator threshold. Outputs: PWM, TBCTR, CMPA Active,
Shadow, Trip, SOC count, event count; remaining fields reserved for M2/M3.

CSV kinds 0..17 are defined in `dsp_behavior.h`. Each row is a committed
event, followed by the final state at that accepted solver step. For several
events at the same timestamp, the `kind/data` sequence gives event ordering;
state columns show the final step snapshot, not intermediate transient state.

## M2 ADC / interrupt contract

Channel order: IL -> VBUS -> rectified VIN. One sequential converter, common
configurable acquisition/conversion times (ns), N=1..16 per channel. ZERO
SOCA starts the first acquisition, each EOC starts the next independent
acquisition. Each pin is read only at its acquisition-end event. The held
digital code survives later input changes through conversion. Quantization:
`clip(round(Vpin*4096/Vref),0,4095)`; full-scale decode uses 4096 bins,
Vref=3.3V. An exact 480V full-scale input clips to 4095 (479.8828125V decode).
N quantized results are summed and rounded to the nearest integer average.
ADC_RESULT (18) records averages; ACQ_START (16) records channel/sample index;
ACQ_END/EOC pack channel in high 16 bits and code in low 16 bits.

No SOC queue: new ZERO while ADC busy increments ADC_OVERRUN and drops the
request. At final channel EOC, ADC_INTERRUPT is requested. One waiting IRQ
snapshot is allowed while ISR runs. A subsequent interrupt while this slot
is occupied increments ISR_OVERRUN and drops the new snapshot; the old one
survives. Latency runs from EOC, so a waiting IRQ enters at the later of its
ready time and prior ISR exit. No preemption or instruction-cycle simulation.

Tie priority extends to: Trip/disable -> ZERO/load/SOCA -> CMPA -> PRD ->
ADC acquisition/EOC -> ISR Shadow write/exit -> ready ISR entry/computation.
ZERO coinciding with last EOC still observes the old busy ADC; that SOC is
dropped. A write coinciding with ZERO misses that load. ISR entry freezes its
ADC image and command, executes for configured ns, then publishes only at
exit. M2 uses an explicit fixture CMPA command; M3 connects the separate AVG
controller at the same entry point.

M2 example: ACQ=80ns, conversion=250ns, N=1, latency=120ns,
execution=800ns. Channel S&H at 80/410/740ns; EOC at 330/660/990ns;
ISR enters at 1110ns and writes/exits at 1910ns; load at 15150ns.
These are simulation assumptions. A user's ACQPS/ADC-clock settings can be
translated to acquisition ns as `(ACQPS+1)*ADC_clock_period_ns`; no claim
about actual F280039C trigger synchronization/aperture timing is made.

Config vector: `[TBCLK_ns,TBPRD,initial_CMPA,ADC_enable,ACQ_ns,conversion_ns,
N,ISR_latency_ns,ISR_execution_ns,Vref_V]`; a 3-field PWM-only vector remains
valid. Output fields 8..16: IL/VBUS/VIN ADC result, ISR running, ISR count,
ADC overrun count, ISR overrun count, pending IRQ, ADC phase (idle/acq/convert).
Maximum same-step event burst is below the fixed 32-event buffer: positive
acquisition/conversion and ISR execution times prohibit unbounded cascades.

## Latest power/measurement specification (M3)

220 Vrms/50Hz, 400V, 3300W, L=220uH, C=1200uF, 66kHz target.
Voltage scale 3.3/480 V/V for rectified AC and bus. Current scale 0.05 V/A,
unity gain, 500kHz bandwidth, configurable zero offset. Comparator propagation
30ns, input error +1mV, HIGH=3.3V. These are user-specified teaching parameters,
not measured hardware characteristics. The 50mOhm scaling is not implicitly
inserted as a dissipative component in the plant.
