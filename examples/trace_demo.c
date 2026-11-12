#include "dsp_behavior.h"

#include <stdio.h>

/* Standalone *peripheral* demonstration, not a Boost PFC power model. */
typedef struct {
    dsp_model_t *model;
} demo_context_t;

static double sample_pin(void *context, dsp_adc_ch_t channel, uint64_t t)
{
    (void)context;
    (void)t;
    return channel == DSP_CH_IL ? 1.65 : 1.10;
}

static uint16_t sample_controller(void *context, uint16_t il, uint16_t vb,
                                  uint64_t t)
{
    (void)context;
    (void)il;
    (void)vb;
    (void)t;
    return 600U; /* Fixed demonstration: real PFC PI controller comes later. */
}

static const char *name_for_event(dsp_event_kind_t kind)
{
    static const char *names[] = {
        "ZERO", "SHADOW_LOAD", "CMPA", "SOCA", "ACQ_END", "EOC",
        "ADC_INTERRUPT", "ISR_ENTRY", "ISR_EXIT", "SHADOW_WRITE",
        "PWM_OUTPUT", "TRIP", "TRIP_CLEAR", "ADC_OVERRUN", "ISR_OVERRUN"
    };
    return (unsigned)kind < (sizeof(names) / sizeof(names[0])) ?
        names[kind] : "UNKNOWN";
}

static void csv_trace(void *context, dsp_event_t event)
{
    demo_context_t *demo = (demo_context_t *)context;
    printf("%llu,%s,%lu,%u,%u,%u,%u\n",
           (unsigned long long)event.time_ns,
           name_for_event(event.kind), (unsigned long)event.data,
           dsp_pwm_output(demo->model) ? 1U : 0U,
           (unsigned)demo->model->cmpa_active,
           (unsigned)demo->model->cmpa_shadow,
           (unsigned)dsp_tbctr(demo->model));
}

int main(void)
{
    dsp_model_t model;
    demo_context_t demo;
    dsp_config_t cfg = {
        .tbclk_ns = 10U,
        .tbprd = 1999U,
        .initial_cmpa = 1000U,
        .acq_ns = {80U, 80U},
        .conversion_ns = {250U, 250U},
        .oversampling = {4U, 1U},
        .isr_latency_ns = 120U,
        .isr_execution_ns = 800U,
        .adc_vref_v = 3.3,
        .ocp_high_v = 2.0,
        .ocp_hysteresis_v = 0.1
    };
    demo.model = &model;
    if (dsp_init(&model, &cfg, sample_pin, sample_controller, csv_trace,
                 &demo) != 0) {
        return 1;
    }
    puts("time_ns,event,data,pwm_out,cmpa_active,cmpa_shadow,tbctr");
    if (dsp_request_pwm_enable(&model, 0U, true) != 0 ||
        dsp_run_until(&model, 50000U) != 0) {
        return 2;
    }
    return 0;
}
