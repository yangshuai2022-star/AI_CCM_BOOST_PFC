#include "dsp_behavior.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    dsp_model_t *model;
    double il_v;
    double bus_v;
    uint16_t target_cmpa;
    uint16_t received_il;
    uint16_t received_bus;
    dsp_event_t events[1024];
    unsigned used;
} fixture_t;

static double read_adc(void *ctx, dsp_adc_ch_t channel, uint64_t time_ns)
{
    fixture_t *f = (fixture_t *)ctx;
    (void)time_ns;
    return channel == DSP_CH_IL ? f->il_v : f->bus_v;
}

static uint16_t controller(void *ctx, uint16_t il, uint16_t bus, uint64_t now)
{
    fixture_t *f = (fixture_t *)ctx;
    (void)now;
    f->received_il = il;
    f->received_bus = bus;
    return f->target_cmpa;
}

static void trace(void *ctx, dsp_event_t event)
{
    fixture_t *f = (fixture_t *)ctx;
    if (f->used < sizeof(f->events) / sizeof(f->events[0])) {
        f->events[f->used++] = event;
    }
}

static dsp_config_t base_config(void)
{
    dsp_config_t c;
    (void)memset(&c, 0, sizeof(c));
    c.tbclk_ns = 10U;
    c.tbprd = 1999U;      /* 50 kHz: 2000 * 10 ns */
    c.initial_cmpa = 1000U;
    c.acq_ns[DSP_CH_IL] = 80U;
    c.conversion_ns[DSP_CH_IL] = 250U;
    c.oversampling[DSP_CH_IL] = 4U;
    c.acq_ns[DSP_CH_VBUS] = 80U;
    c.conversion_ns[DSP_CH_VBUS] = 250U;
    c.oversampling[DSP_CH_VBUS] = 1U;
    c.isr_latency_ns = 120U;
    c.isr_execution_ns = 800U;
    c.adc_vref_v = 3.3;
    c.ocp_high_v = 2.0;
    c.ocp_hysteresis_v = 0.1;
    return c;
}

static void setup(dsp_model_t *m, fixture_t *f, const dsp_config_t *c)
{
    (void)memset(f, 0, sizeof(*f));
    f->model = m;
    f->il_v = 1.65;
    f->bus_v = 1.1;
    f->target_cmpa = 600U;
    assert(dsp_init(m, c, read_adc, controller, trace, f) == 0);
    assert(!dsp_pwm_output(m)); /* startup is guaranteed off */
}

static bool has_event(const fixture_t *f, dsp_event_kind_t kind,
                      uint64_t at, uint32_t data)
{
    unsigned i;
    for (i = 0U; i < f->used; i++) {
        if (f->events[i].kind == kind && f->events[i].time_ns == at &&
            f->events[i].data == data) {
            return true;
        }
    }
    return false;
}

static void test_zero_soca_sampling_isr_shadow(void)
{
    dsp_model_t m;
    fixture_t f;
    dsp_config_t c = base_config();
    setup(&m, &f, &c);
    assert(dsp_request_pwm_enable(&m, 0U, true) == 0);
    assert(dsp_pwm_output(&m));
    assert(dsp_next_event_ns(&m) == 80U);
    assert(has_event(&f, DSP_EVENT_ZERO, 0U, 0U));
    assert(has_event(&f, DSP_EVENT_SOCA, 0U, 0U));

    assert(dsp_run_until(&m, 2569U) == 0);
    assert(m.cmpa_shadow_pending == false);
    assert(m.cmpa_active == 1000U);
    assert(f.received_il == 2048U); /* 1.65 / 3.3 -> 2047.5 rounded */
    assert(f.received_bus == 1365U);
    assert(has_event(&f, DSP_EVENT_ACQ_END, 80U, 2048U));
    assert(has_event(&f, DSP_EVENT_EOC, 330U, 2048U));
    assert(has_event(&f, DSP_EVENT_EOC, 1650U, (1U << 16U) | 1365U));
    assert(has_event(&f, DSP_EVENT_ADC_INTERRUPT, 1650U, 0U));
    assert(has_event(&f, DSP_EVENT_ISR_ENTRY, 1770U, 0U));
    assert(!has_event(&f, DSP_EVENT_SHADOW_WRITE, 2569U, 600U));

    assert(dsp_run_until(&m, 2570U) == 0);
    assert(m.cmpa_shadow_pending);
    assert(m.cmpa_shadow == 600U);
    assert(has_event(&f, DSP_EVENT_SHADOW_WRITE, 2570U, 600U));
    assert(m.cmpa_active == 1000U); /* no immediate active-register change */
    assert(dsp_run_until(&m, 10000U) == 0);
    assert(has_event(&f, DSP_EVENT_CMPA, 10000U, 1000U));
    assert(!dsp_pwm_output(&m));
    assert(dsp_run_until(&m, 20000U) == 0);
    assert(m.cmpa_active == 600U);
    assert(dsp_pwm_output(&m));
    assert(has_event(&f, DSP_EVENT_SHADOW_LOAD, 20000U, 600U));
    assert(dsp_run_until(&m, 26000U) == 0);
    assert(!dsp_pwm_output(&m));
    assert(has_event(&f, DSP_EVENT_CMPA, 26000U, 600U));
}

static void test_late_isr_misses_zero(void)
{
    dsp_model_t m;
    fixture_t f;
    dsp_config_t c = base_config();
    /* Last EOC=1650, ISR entry=1770, exit at exactly ZERO=20000. */
    c.isr_execution_ns = 18230U;
    setup(&m, &f, &c);
    assert(dsp_request_pwm_enable(&m, 0U, true) == 0);
    assert(dsp_run_until(&m, 20000U) == 0);
    assert(m.cmpa_active == 1000U);
    assert(m.cmpa_shadow == 600U);
    assert(m.cmpa_shadow_pending);
    assert(has_event(&f, DSP_EVENT_SHADOW_WRITE, 20000U, 600U));
    assert(!has_event(&f, DSP_EVENT_SHADOW_LOAD, 20000U, 600U));
    assert(dsp_run_until(&m, 40000U) == 0);
    assert(m.cmpa_active == 600U);
    assert(has_event(&f, DSP_EVENT_SHADOW_LOAD, 40000U, 600U));
}

static void test_asynchronous_trip_and_rearm(void)
{
    dsp_model_t m;
    fixture_t f;
    dsp_config_t c = base_config();
    setup(&m, &f, &c);
    assert(dsp_request_pwm_enable(&m, 0U, true) == 0);
    assert(dsp_pwm_output(&m));
    assert(dsp_set_comparator_voltage(&m, 1234U, 2.2) == 0);
    assert(m.trip_latched && m.comparator_high);
    assert(!dsp_pwm_output(&m));
    assert(has_event(&f, DSP_EVENT_TRIP, 1234U, 1U));
    assert(has_event(&f, DSP_EVENT_PWM_OUTPUT, 1234U, 0U));
    assert(dsp_request_trip_clear(&m, 1240U) == -3);
    assert(m.trip_latched);
    assert(dsp_set_comparator_voltage(&m, 1300U, 1.8) == 0);
    assert(!m.comparator_high && m.trip_latched);
    assert(dsp_request_trip_clear(&m, 1300U) == 0);
    assert(dsp_run_until(&m, 19999U) == 0);
    assert(!dsp_pwm_output(&m));
    assert(dsp_run_until(&m, 20000U) == 0);
    assert(!m.trip_latched);
    assert(dsp_pwm_output(&m));
    assert(has_event(&f, DSP_EVENT_TRIP_CLEAR, 20000U, 0U));
}

static void test_adc_input_clipping_and_overrun(void)
{
    dsp_model_t m;
    fixture_t f;
    dsp_config_t c = base_config();
    setup(&m, &f, &c);
    f.il_v = -2.0;
    f.bus_v = 5.0;
    assert(dsp_run_until(&m, 2570U) == 0);
    assert(f.received_il == 0U);
    assert(f.received_bus == 4095U);

    c.acq_ns[DSP_CH_IL] = 5000U;
    c.conversion_ns[DSP_CH_IL] = 5000U;
    c.acq_ns[DSP_CH_VBUS] = 5000U;
    c.conversion_ns[DSP_CH_VBUS] = 5000U;
    setup(&m, &f, &c);
    assert(dsp_run_until(&m, 40000U) == 0);
    assert(m.adc_overrun_count == 2U); /* SOCA at 20us, 40us while busy */
    assert(m.soc_count == 3U);
    assert(m.adc_phase != 0U);
}

static void test_isr_overrun_and_no_cpu_trip_dependency(void)
{
    dsp_model_t m;
    fixture_t f;
    dsp_config_t c = base_config();
    c.isr_execution_ns = 50000U;
    setup(&m, &f, &c);
    assert(dsp_request_pwm_enable(&m, 0U, true) == 0);
    assert(dsp_run_until(&m, 41000U) == 0);
    assert(m.isr_running);
    assert(m.isr_pending);
    assert(dsp_run_until(&m, 41700U) == 0);
    assert(m.isr_overrun_count == 1U); /* IRQ latch already pending */
    /* Trip works immediately even with a stalled/occupied CPU. */
    assert(dsp_set_comparator_voltage(&m, 41701U, 2.5) == 0);
    assert(!dsp_pwm_output(&m));
    assert(m.trip_latched);
}

static void test_timer_count_and_invalid_config(void)
{
    dsp_model_t m;
    fixture_t f;
    dsp_config_t c = base_config();
    setup(&m, &f, &c);
    assert(dsp_run_until(&m, 0U) == 0);
    assert(dsp_tbctr(&m) == 0U);
    assert(dsp_run_until(&m, 505U) == 0);
    assert(dsp_tbctr(&m) == 50U);
    assert(dsp_run_until(&m, 20000U) == 0);
    assert(dsp_tbctr(&m) == 0U);
    assert(dsp_run_until(&m, 19999U) != 0); /* no time travel */
    c.oversampling[DSP_CH_IL] = 0U;
    assert(dsp_init(&m, &c, read_adc, controller, trace, &f) != 0);
}

int main(void)
{
    test_zero_soca_sampling_isr_shadow();
    test_late_isr_misses_zero();
    test_asynchronous_trip_and_rearm();
    test_adc_input_clipping_and_overrun();
    test_isr_overrun_and_no_cpu_trip_dependency();
    test_timer_count_and_invalid_config();
    puts("PASS: 6 digital-peripheral behavior regression tests");
    return 0;
}
