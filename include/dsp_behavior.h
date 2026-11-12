#ifndef DSP_BEHAVIOR_H
#define DSP_BEHAVIOR_H
#include <stdbool.h>
#include <stdint.h>
/* Host only; floating ns preserves off-clock Trip. TBCLK timers remain on
 * the clock grid. Caller visits each deadline. No allocation or callbacks. */
#define DSP_TIME_NEVER (1.0e100)
#define DSP_EVENT_CAPACITY (32U)
#define DSP_INPUT_WIDTH (8)
#define DSP_OUTPUT_WIDTH (16)
typedef enum {
    DSP_EVENT_ZERO=0, DSP_EVENT_SHADOW_LOAD, DSP_EVENT_CMPA,
    DSP_EVENT_SOCA, DSP_EVENT_ACQ_END, DSP_EVENT_EOC,
    DSP_EVENT_ADC_INTERRUPT, DSP_EVENT_ISR_ENTRY, DSP_EVENT_ISR_EXIT,
    DSP_EVENT_SHADOW_WRITE, DSP_EVENT_PWM_OUTPUT, DSP_EVENT_TRIP,
    DSP_EVENT_TRIP_CLEAR, DSP_EVENT_ADC_OVERRUN, DSP_EVENT_ISR_OVERRUN,
    DSP_EVENT_PRD, DSP_EVENT_ACQ_START, DSP_EVENT_CONTROL
} dsp_event_kind_t;
typedef struct { dsp_event_kind_t kind; double time_ns; uint32_t data; } dsp_event_t;
typedef struct { uint32_t tbclk_ns; uint16_t tbprd, initial_cmpa; } dsp_config_t;
typedef struct {
    dsp_config_t cfg;
    double now_ns, last_zero_ns, next_zero_ns, next_cmpa_ns, next_prd_ns;
    uint16_t cmpa_active, cmpa_shadow;
    uint32_t command_seq, soc_count, event_count;
    bool command_seen, cmpa_shadow_pending, comparator_high;
    bool trip_latched, trip_clear_requested, clear_previous;
    bool enable_requested, pwm_enabled, aq_high, pwm_out;
    dsp_event_t events[DSP_EVENT_CAPACITY]; uint32_t event_size;
} dsp_model_t;
int dsp_init(dsp_model_t *m, const dsp_config_t *cfg);
double dsp_next_event_ns(const dsp_model_t *m);
/* IL pin V, VBUS pin V, OC logic V, safe enable 0/1, clear rising edge,
 * CMPA ticks, write sequence, VIN pin V. Digital HIGH >=1.65V.
 * Trip before timers; ZERO before same-time shadow writes.
 * Invalid/nonfinite inputs and skipped deadlines return nonzero. */
int dsp_step(dsp_model_t *m, double time_ns, const double input[DSP_INPUT_WIDTH]);
void dsp_outputs(const dsp_model_t *m, double time_ns, double output[DSP_OUTPUT_WIDTH]);
#endif
