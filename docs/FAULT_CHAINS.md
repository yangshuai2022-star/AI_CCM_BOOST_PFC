# Fault-chain review evidence

All VERIFIED tags below refer to MATLAB/Simulink runs, not native C tests or
real hardware. The review follows each requested event path and its consumer.

## Trip [ALIVE]

Root: OC_COMP_OUT reaches HIGH.
Chain: Simulink sense state [VERIFIED] -> +1mV threshold crossing [VERIFIED]
-> 30ns delayed comparator output [VERIFIED] -> digital HIGH [VERIFIED]
-> Trip latch [VERIFIED] -> actual PWM LOW [VERIFIED], before ADC/CPU effects.
Evidence: M3 analog_trip_events.csv / analog_trip.png; raw 969.504793889ns,
OC/PWM-low 999.504793889ns. Full power-stage current triggers at
1.328410936ms in pfc_trip_events.csv.gz. M2 isr_overrun_trip logs shutdown
at 31333.333ns while a 50us ISR is still active.
Exclusion: no ADC/EOC/ISR shutdown dependency; release does not clear latch.
Falsification: any high gate after the logged HIGH while Trip is latched,
or Trip/PWM low more than 1ps later than the comparator transition.

## Safe explicit recovery [ALIVE]

Root: comparator releases, software clear may be requested.
Chain: no new clear edge -> latch persists [VERIFIED]; clear held across fault
-> no auto restart [VERIFIED]; clear while safe enable=0 -> rejected
[VERIFIED]; eligible new edge -> pending clear -> ZERO clear [VERIFIED].
Evidence: M1 trip / held_clear / unsafe_clear / trip_zero CSVs. Trip and ZERO
at the same timestamp never produce a high pulse.
Exclusion: a low comparator alone and a stale held clear cannot grant restart.
Falsification: PWM rises without an eligible explicit clear and safe enable.

## SOC loss / ADC busy [ALIVE diagnostics; DEAD automatic shutdown]

Root: conversion group exceeds the next PWM SOCA interval.
Chain: ZERO -> SOCA -> busy ADC -> increment ADC_OVERRUN -> discard new SOC,
old acquisition/conversion continues -> old group EOC [all VERIFIED].
Evidence: M2 adc_overrun_events.csv, 10us acquisition +10us conversion per
channel; overruns at 15.15/30.30/45.45us, original group completes at 60us.
Physical consequence: feedback can be stale and control rate lower; this
teaching model records it but does not invent an automatic fault policy.
Other path: independent digital Trip remains ALIVE; not dependent on ADC.
Falsification: missed count, silently replaced held sample, or falsely
claiming that this counter itself forces gate-off.

## EOC/ISR backlog [ALIVE diagnostics; DEAD automatic shutdown]

Root: a 50us ISR cannot keep up with 15.15us SOC groups.
Chain: final EOC -> pending image -> ISR busy -> preserve one waiting image
-> next EOC while waiting slot full -> increment ISR_OVERRUN/drop new image
-> old ISR exit -> enter pending ISR using old image [all VERIFIED].
Evidence: M2 isr_overrun_trip_events.csv; second waiting entry at 51110ns
contains the 0.4V sample, despite subsequent 0.8V ADC values.
Physical consequence: bounded data loss and stale control command are visible,
not a validated overloaded control mode. There is no automatic CPU-stall
watchdog here. Trip still forces PWM off independently (verified in same run).
Falsification: new EOC silently overwrites the pending snapshot or ISR backlog
prevents the digital Trip from forcing PWM low.

## Shadow misses ZERO [ALIVE]

Root: ISR Shadow write arrives before / exactly at / after ZERO.
Chain: old Active compare continues -> ZERO loads prior Shadow -> later CPU
write becomes pending -> next qualifying ZERO loads it -> CMPA edge changes
[all VERIFIED]. Evidence: M2 isr_15140ns / isr_15150ns / isr_15160ns CSVs and
M1 external-write timing tests. 15140ns write loads at 15150ns; 15150/15160ns
writes wait until 30300ns, then CMPA=400 clears at 34300ns.
Physical consequence: one additional switching period of actuation delay.
Exclusion: controller cannot write Active or retroactively change an edge.
Falsification: same-time write is loaded at the ZERO it coincides with.

## Trip during precomputed ISR [ALIVE]

Root: ISR computed a nonzero CMPA before an asynchronous Trip.
Chain: entry at 1110ns computes CMPA=537 -> Trip at 1500.123ns resets staged
command/PI/Shadow -> ISR exit at 1910ns writes zero -> explicit eligible clear
at ZERO 15150ns loads zero -> fresh ADC/ISR needed for later PWM
[all VERIFIED in M3 control_reset_events.csv and actual output assertions].
Exclusion: same ISR's old command cannot survive a latch clear. Fault handling
also resets integral power/duty states. This is model behavior, not firmware
register timing evidence. Falsification: 537 or a nonzero old duty reappears at
exit/rearm before fresh computation.

## Source event visibility [ALIVE within tested solver contract]

Root: external source generates a narrow pulse between large solver steps.
A nonadvertised pulse can have no sign change at trial endpoints and be
missed (observed during review). Fixed stimulus chain: explicit left/right
values at duplicate timestamps -> From Workspace discontinuity ZC -> accepted
major event -> digital Trip latch [VERIFIED by the one-microsecond unsafe-clear
case]. Analog comparator events arise from continuous states plus comparator
ZC. No TBCLK-grid polling is used to discover Trip.
Exclusion: this does not guarantee arbitrary unresolved pulses from an
incorrectly modeled external source. Falsification: no Trip event for the
advertised one-microsecond HIGH pulse.

## Unknown hardware paths [UNKNOWN]

Comparator stuck LOW, real wire faults, real ADC/PIE/TZ timing, physical current
polarity and real MCU failure modes have not been tested. This model contains
no independent hardware watchdog, relay isolation or redundant overcurrent
path. The virtual Boost diode can conduct with gate LOW. These exclusions
prevent a simulation PASS from being presented as hardware protection proof.
