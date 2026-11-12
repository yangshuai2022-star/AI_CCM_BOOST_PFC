#include "dsp_behavior.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define DSP_ADC_IDLE (0U)
#define DSP_ADC_ACQUIRE (1U)
#define DSP_ADC_CONVERT (2U)

static uint64_t later_than(uint64_t a, uint64_t b)
{
    return a > b ? a : b;
}

static uint64_t earliest(uint64_t a, uint64_t b)
{
    return a < b ? a : b;
}

static void log_event(dsp_model_t *m, dsp_event_kind_t kind, uint32_t data)
{
    if (m->trace != NULL) {
        dsp_event_t e;
        e.kind = kind;
        e.time_ns = m->now_ns;
        e.data = data;
        m->trace(m->user_context, e);
    }
}

static void refresh_pwm(dsp_model_t *m)
{
    const bool new_output = m->pwm_enabled && !m->trip_latched &&
                            !m->comparator_high && m->aq_high;
    if (new_output != m->pwm_out) {
        m->pwm_out = new_output;
        log_event(m, DSP_EVENT_PWM_OUTPUT, new_output ? 1U : 0U);
    }
}

static uint16_t quantize_12bit(const dsp_model_t *m, double adc_pin_v)
{
    /* Model ADC clipping + nearest-code quantization, not analog ENOB/noise. */
    if (!(adc_pin_v > 0.0)) {
        return 0U;             /* includes negative values and NaN */
    }
    if (adc_pin_v >= m->cfg.adc_vref_v) {
        return DSP_ADC_MAX_CODE;
    }
    return (uint16_t)floor((adc_pin_v * (double)DSP_ADC_MAX_CODE /
                            m->cfg.adc_vref_v) + 0.5);
}

static void begin_acquisition(dsp_model_t *m)
{
    const uint32_t ch = (uint32_t)m->adc_channel;
    m->adc_phase = DSP_ADC_ACQUIRE;
    m->adc_event_ns = m->now_ns + (uint64_t)m->cfg.acq_ns[ch];
}

static void issue_soca(dsp_model_t *m)
{
    log_event(m, DSP_EVENT_SOCA, 0U);
    m->soc_count++;
    if (m->adc_phase != DSP_ADC_IDLE) {
        m->adc_overrun_count++;
        log_event(m, DSP_EVENT_ADC_OVERRUN, m->adc_overrun_count);
        return;
    }
    m->adc_sum[0] = 0U;
    m->adc_sum[1] = 0U;
    m->adc_sample_index = 0U;
    m->adc_channel = 0U;
    begin_acquisition(m);
}

static void handle_zero(dsp_model_t *m)
{
    const uint64_t full_period = ((uint64_t)m->cfg.tbprd + 1U) *
                                 (uint64_t)m->cfg.tbclk_ns;
    m->last_zero_ns = m->now_ns;
    m->next_zero_ns = m->now_ns + full_period;

    /* Hardware loads before CPU writes at an identical zero timestamp. */
    if (m->cmpa_shadow_pending) {
        m->cmpa_active = m->cmpa_shadow;
        m->cmpa_shadow_pending = false;
        log_event(m, DSP_EVENT_SHADOW_LOAD, m->cmpa_active);
    }
    if (m->trip_clear_requested && !m->comparator_high) {
        m->trip_latched = false;
        m->trip_clear_requested = false;
        log_event(m, DSP_EVENT_TRIP_CLEAR, 0U);
    }
    if (m->enable_requested) {
        m->pwm_enabled = true;
    }
    /* AQ set on zero and clear on CMPA, including 0% and 100% bounds. */
    m->aq_high = m->cmpa_active > 0U;
    m->next_cmpa_ns = DSP_TIME_NEVER;
    if (m->cmpa_active > 0U) {
        m->next_cmpa_ns = m->now_ns + (uint64_t)m->cmpa_active *
                                       (uint64_t)m->cfg.tbclk_ns;
    }
    log_event(m, DSP_EVENT_ZERO, 0U);
    refresh_pwm(m);
    issue_soca(m);
}

static void handle_adc(dsp_model_t *m)
{
    const uint32_t ch = m->adc_channel;
    if (m->adc_phase == DSP_ADC_ACQUIRE) {
        const double v = m->read_adc(m->user_context, (dsp_adc_ch_t)ch,
                                      m->now_ns);
        m->adc_hold = quantize_12bit(m, v);
        log_event(m, DSP_EVENT_ACQ_END, (ch << 16U) | m->adc_hold);
        m->adc_phase = DSP_ADC_CONVERT;
        m->adc_event_ns = m->now_ns + (uint64_t)m->cfg.conversion_ns[ch];
        return;
    }
    if (m->adc_phase != DSP_ADC_CONVERT) {
        return;
    }

    m->adc_sum[ch] += m->adc_hold;
    log_event(m, DSP_EVENT_EOC, (ch << 16U) | m->adc_hold);
    m->adc_sample_index++;
    if (m->adc_sample_index < m->cfg.oversampling[ch]) {
        begin_acquisition(m);
        return;
    }
    m->adc_result[ch] = (uint16_t)(
        (m->adc_sum[ch] + m->cfg.oversampling[ch] / 2U) /
         m->cfg.oversampling[ch]);
    if (ch == DSP_CH_IL) {
        m->adc_channel = DSP_CH_VBUS;
        m->adc_sample_index = 0U;
        begin_acquisition(m);
        return;
    }
    m->adc_event_ns = DSP_TIME_NEVER;
    m->adc_phase = DSP_ADC_IDLE;
    log_event(m, DSP_EVENT_ADC_INTERRUPT, 0U);
    if (m->isr_pending) {
        /* One pending IRQ latch in this v0.1 model; a second one is lost. */
        m->isr_overrun_count++;
        log_event(m, DSP_EVENT_ISR_OVERRUN, m->isr_overrun_count);
        return;
    }
    m->pending_il_code = m->adc_result[DSP_CH_IL];
    m->pending_vbus_code = m->adc_result[DSP_CH_VBUS];
    m->isr_pending = true;
    m->isr_ready_ns = m->now_ns + (uint64_t)m->cfg.isr_latency_ns;
}

static void handle_isr_entry(dsp_model_t *m)
{
    m->isr_pending = false;
    m->isr_running = true;
    m->isr_ready_ns = DSP_TIME_NEVER;
    m->isr_count++;
    log_event(m, DSP_EVENT_ISR_ENTRY, 0U);
    /* Compute using the sampled ADC register image at ISR entry.
     * Commit its CMPA write only when the modeled ISR has finished.
     */
    m->staged_cmpa = m->controller(m->user_context, m->pending_il_code,
                                   m->pending_vbus_code, m->now_ns);
    if (m->staged_cmpa > m->cfg.tbprd) {
        m->staged_cmpa = m->cfg.tbprd;
    }
    m->isr_finish_ns = m->now_ns + (uint64_t)m->cfg.isr_execution_ns;
}

static void handle_isr_exit(dsp_model_t *m)
{
    m->isr_running = false;
    m->isr_finish_ns = DSP_TIME_NEVER;
    m->cmpa_shadow = m->staged_cmpa;
    m->cmpa_shadow_pending = true;
    log_event(m, DSP_EVENT_SHADOW_WRITE, m->cmpa_shadow);
    log_event(m, DSP_EVENT_ISR_EXIT, 0U);
}

int dsp_init(dsp_model_t *m, const dsp_config_t *cfg,
             dsp_read_adc_fn read_adc, dsp_controller_fn controller,
             dsp_trace_fn trace, void *user_context)
{
    uint32_t ch;
    if (m == NULL || cfg == NULL || read_adc == NULL || controller == NULL ||
        cfg->tbclk_ns == 0U || cfg->tbprd < 2U ||
        cfg->initial_cmpa > cfg->tbprd || !isfinite(cfg->adc_vref_v) ||
        cfg->adc_vref_v <= 0.0 || !isfinite(cfg->ocp_high_v) ||
        cfg->ocp_high_v <= 0.0 || !isfinite(cfg->ocp_hysteresis_v) ||
        cfg->ocp_hysteresis_v < 0.0 ||
        cfg->ocp_hysteresis_v >= cfg->ocp_high_v) {
        return -1;
    }
    for (ch = 0U; ch < DSP_ADC_CH_COUNT; ch++) {
        if (cfg->acq_ns[ch] == 0U || cfg->conversion_ns[ch] == 0U ||
            cfg->oversampling[ch] == 0U || cfg->oversampling[ch] > 16U) {
            return -1;
        }
    }
    (void)memset(m, 0, sizeof(*m));
    m->cfg = *cfg;
    m->read_adc = read_adc;
    m->controller = controller;
    m->trace = trace;
    m->user_context = user_context;
    m->cmpa_active = cfg->initial_cmpa;
    m->cmpa_shadow = cfg->initial_cmpa;
    m->next_zero_ns = 0U;     /* first ZERO is a real scheduled event */
    m->next_cmpa_ns = DSP_TIME_NEVER;
    m->adc_event_ns = DSP_TIME_NEVER;
    m->isr_ready_ns = DSP_TIME_NEVER;
    m->isr_finish_ns = DSP_TIME_NEVER;
    return 0;
}

uint64_t dsp_next_event_ns(const dsp_model_t *m)
{
    uint64_t next;
    if (m == NULL) {
        return DSP_TIME_NEVER;
    }
    next = earliest(m->next_zero_ns, m->next_cmpa_ns);
    next = earliest(next, m->adc_event_ns);
    if (m->isr_running) {
        next = earliest(next, m->isr_finish_ns);
    } else if (m->isr_pending) {
        next = earliest(next, later_than(m->isr_ready_ns, m->now_ns));
    }
    return next;
}

/* Every iteration processes one event, so coincident timestamps are ordered:
 * ZERO/load > CMPA > ADC EOC/ACQ > ISR finish > ISR entry. In particular,
 * an ISR write at exactly ZERO misses that ZERO's shadow-load opportunity.
 */
static int advance_internal(dsp_model_t *m, uint64_t end_ns, bool inclusive)
{
    uint32_t iterations = 0U;
    uint64_t next;
    if (m == NULL || end_ns < m->now_ns) {
        return -1;
    }
    while ((next = dsp_next_event_ns(m)) != DSP_TIME_NEVER &&
           (inclusive ? next <= end_ns : next < end_ns)) {
        if (++iterations == 10000000U || next < m->now_ns) {
            return -2;
        }
        m->now_ns = next;
        if (m->next_zero_ns == next) {
            handle_zero(m);
        } else if (m->next_cmpa_ns == next) {
            m->next_cmpa_ns = DSP_TIME_NEVER;
            m->aq_high = false;
            log_event(m, DSP_EVENT_CMPA, m->cmpa_active);
            refresh_pwm(m);
        } else if (m->adc_event_ns == next) {
            handle_adc(m);
        } else if (m->isr_running && m->isr_finish_ns == next) {
            handle_isr_exit(m);
        } else if (m->isr_pending && !m->isr_running &&
                   m->isr_ready_ns <= next) {
            handle_isr_entry(m);
        } else {
            return -3;
        }
    }
    m->now_ns = end_ns;
    return 0;
}

int dsp_run_until(dsp_model_t *m, uint64_t end_ns)
{
    return advance_internal(m, end_ns, true);
}

int dsp_set_comparator_voltage(dsp_model_t *m, uint64_t time_ns,
                               double comparator_input_v)
{
    int status;
    if (m == NULL || !isfinite(comparator_input_v)) {
        return -1;
    }
    status = advance_internal(m, time_ns, false);
    if (status != 0) {
        return status;
    }
    if ((!m->comparator_high &&
         comparator_input_v >= m->cfg.ocp_high_v) ||
        (m->comparator_high &&
         comparator_input_v > m->cfg.ocp_high_v - m->cfg.ocp_hysteresis_v)) {
        m->comparator_high = true;
        if (!m->trip_latched) {
            m->trip_latched = true;
            m->trip_clear_requested = false;
            log_event(m, DSP_EVENT_TRIP, 1U);
        }
    } else {
        m->comparator_high = false;
    }
    refresh_pwm(m); /* No ISR latency and no TBCLK quantization on trip. */
    return advance_internal(m, time_ns, true);
}

int dsp_request_pwm_enable(dsp_model_t *m, uint64_t time_ns, bool enable)
{
    int status = advance_internal(m, time_ns, false);
    if (status != 0) {
        return status;
    }
    m->enable_requested = enable;
    if (!enable) {
        m->pwm_enabled = false;
        refresh_pwm(m); /* disable immediately */
    }
    return advance_internal(m, time_ns, true);
}

int dsp_request_trip_clear(dsp_model_t *m, uint64_t time_ns)
{
    int status = advance_internal(m, time_ns, false);
    if (status != 0) {
        return status;
    }
    if (m->comparator_high) {
        (void)advance_internal(m, time_ns, true);
        return -3; /* Cannot clear an active analog comparator. */
    }
    m->trip_clear_requested = true;
    return advance_internal(m, time_ns, true);
}

uint16_t dsp_tbctr(const dsp_model_t *m)
{
    if (m == NULL || m->next_zero_ns == 0U) {
        return 0U;
    }
    return (uint16_t)((m->now_ns - m->last_zero_ns) / m->cfg.tbclk_ns);
}

bool dsp_pwm_output(const dsp_model_t *m)
{
    return m != NULL && m->pwm_out;
}
