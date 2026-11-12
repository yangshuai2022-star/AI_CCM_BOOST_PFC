#ifndef DSP_BEHAVIOR_H
#define DSP_BEHAVIOR_H

/*
 * Portable C99 event-driven digital-peripheral behavior model (v0.1).
 * Timing values are simulated nanoseconds, NOT TI register settings.
 * No allocation, OS, DSP SDK, or dependency on a particular MCU.
 */
#include <stdbool.h>
#include <stdint.h>

#define DSP_ADC_BITS (12U)
#define DSP_ADC_MAX_CODE (4095U)
#define DSP_ADC_CH_COUNT (2U)
#define DSP_TIME_NEVER (UINT64_MAX)

typedef enum {
    DSP_CH_IL = 0,      /* PFC inductor-current sensing ADC pin */
    DSP_CH_VBUS = 1     /* PFC bus-voltage sensing ADC pin */
} dsp_adc_ch_t;

typedef enum {
    DSP_EVENT_ZERO = 0,
    DSP_EVENT_SHADOW_LOAD,
    DSP_EVENT_CMPA,
    DSP_EVENT_SOCA,
    DSP_EVENT_ACQ_END,
    DSP_EVENT_EOC,
    DSP_EVENT_ADC_INTERRUPT,
    DSP_EVENT_ISR_ENTRY,
    DSP_EVENT_ISR_EXIT,
    DSP_EVENT_SHADOW_WRITE,
    DSP_EVENT_PWM_OUTPUT,
    DSP_EVENT_TRIP,
    DSP_EVENT_TRIP_CLEAR,
    DSP_EVENT_ADC_OVERRUN,
    DSP_EVENT_ISR_OVERRUN
} dsp_event_kind_t;

typedef struct {
    dsp_event_kind_t kind;
    uint64_t time_ns;
    uint32_t data;    /* CMPA count, ADC code, channel, or PWM state */
} dsp_event_t;

typedef double (*dsp_read_adc_fn)(void *context, dsp_adc_ch_t ch,
                                  uint64_t acquisition_end_ns);
typedef uint16_t (*dsp_controller_fn)(void *context,
                                      uint16_t il_code, uint16_t vbus_code,
                                      uint64_t isr_entry_ns);
typedef void (*dsp_trace_fn)(void *context, dsp_event_t event);

typedef struct {
    uint32_t tbclk_ns;               /* 10 -> 100 MHz time-base clock */
    uint16_t tbprd;                  /* up-count: period=(TBPRD+1)*TBCLK */
    uint16_t initial_cmpa;           /* AQ: set at ZERO, clear at CMPA */
    uint32_t acq_ns[DSP_ADC_CH_COUNT];
    uint32_t conversion_ns[DSP_ADC_CH_COUNT];
    uint8_t oversampling[DSP_ADC_CH_COUNT]; /* sequential ADC SOCs; 1..16 */
    uint32_t isr_latency_ns;        /* EOC interrupt pending -> ISR entry */
    uint32_t isr_execution_ns;      /* ISR entry -> shadow CMPA write */
    double adc_vref_v;              /* 12-bit unsigned, 0 to Vref */
    double ocp_high_v;              /* independent analog current threshold */
    double ocp_hysteresis_v;        /* deassert comparator below high-hyst */
} dsp_config_t;

typedef struct {
    dsp_config_t cfg;
    void *user_context;
    dsp_read_adc_fn read_adc;
    dsp_controller_fn controller;
    dsp_trace_fn trace;

    uint64_t now_ns;
    uint64_t last_zero_ns;
    uint64_t next_zero_ns;
    uint64_t next_cmpa_ns;
    uint64_t adc_event_ns;
    uint64_t isr_ready_ns;
    uint64_t isr_finish_ns;

    uint16_t cmpa_active;
    uint16_t cmpa_shadow;
    uint16_t staged_cmpa;
    uint16_t adc_result[DSP_ADC_CH_COUNT];
    uint16_t adc_hold;
    uint16_t pending_il_code;
    uint16_t pending_vbus_code;
    uint32_t adc_sum[DSP_ADC_CH_COUNT];
    uint32_t adc_overrun_count;
    uint32_t isr_overrun_count;
    uint32_t soc_count;
    uint32_t isr_count;

    uint8_t adc_channel;
    uint8_t adc_sample_index;
    uint8_t adc_phase;      /* 0 idle, 1 acquisition, 2 conversion */
    bool cmpa_shadow_pending;
    bool isr_pending;
    bool isr_running;
    bool comparator_high;
    bool trip_latched;
    bool trip_clear_requested;
    bool enable_requested;
    bool pwm_enabled;
    bool aq_high;
    bool pwm_out;
} dsp_model_t;

/* Returns 0 on success; non-zero on invalid configuration/arguments. */
int dsp_init(dsp_model_t *m, const dsp_config_t *cfg,
             dsp_read_adc_fn read_adc, dsp_controller_fn controller,
             dsp_trace_fn trace, void *user_context);

/* Next internal hardware/CPU event. The owner/plant should advance to it. */
uint64_t dsp_next_event_ns(const dsp_model_t *m);

/* Process every scheduled event up to and including end_ns. */
int dsp_run_until(dsp_model_t *m, uint64_t end_ns);

/* External plant/comparator input at arbitrary ns (not restricted to TBCLK).
 * In a Simulink adapter, use zero-crossing detection to locate the threshold.
 * Input event is processed BEFORE any internal event at the same timestamp.
 */
int dsp_set_comparator_voltage(dsp_model_t *m, uint64_t time_ns,
                               double comparator_input_v);

/* Disable is immediate; enable is committed at the NEXT ZERO event. */
int dsp_request_pwm_enable(dsp_model_t *m, uint64_t time_ns, bool enable);

/* OST-style software clear: only eligible with comparator released, and
 * becomes effective at the NEXT ZERO. Cannot override a live comparator.
 */
int dsp_request_trip_clear(dsp_model_t *m, uint64_t time_ns);

uint16_t dsp_tbctr(const dsp_model_t *m);
bool dsp_pwm_output(const dsp_model_t *m);

#endif /* DSP_BEHAVIOR_H */
