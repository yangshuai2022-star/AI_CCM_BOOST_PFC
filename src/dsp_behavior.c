#include "dsp_behavior.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
/* 0.1 ps root tolerance, never a TBCLK grid. */
#define EPS_NS (1.0e-4)
static double earliest(double a,double b) { return a<b?a:b; }
static bool due(double t,double at) { return t>=at && t-at<=EPS_NS; }
static void log_event(dsp_model_t *m,dsp_event_kind_t kind,uint32_t data)
{
    if (m->event_size<DSP_EVENT_CAPACITY) {
        dsp_event_t *e=&m->events[m->event_size];
        e->kind=kind; e->time_ns=m->now_ns; e->data=data;
        m->event_size++; m->event_count++;
    }
}
static void refresh_pwm(dsp_model_t *m)
{
    bool output=m->pwm_enabled && !m->trip_latched && !m->comparator_high && m->aq_high;
    if (output!=m->pwm_out) {
        m->pwm_out=output; log_event(m,DSP_EVENT_PWM_OUTPUT,output?1U:0U);
    }
}
static void begin_acquisition(dsp_model_t *m)
{
    m->adc_phase=1U; m->adc_event_ns=m->now_ns+m->cfg.acq_ns;
    log_event(m,DSP_EVENT_ACQ_START,((uint32_t)m->adc_channel<<16U)|m->adc_sample_index);
}
static void issue_soca(dsp_model_t *m)
{
    m->soc_count++; log_event(m,DSP_EVENT_SOCA,0U);
    if (!m->cfg.adc_enabled) { return; }
    if (m->adc_phase!=0U) {
        m->adc_overrun_count++; log_event(m,DSP_EVENT_ADC_OVERRUN,m->adc_overrun_count); return;
    }
    m->adc_channel=0U; m->adc_sample_index=0U; m->adc_sum=0U;
    begin_acquisition(m);
}
static uint16_t quantize_12bit(double pin,double vref)
{
    double code=floor(pin*4096.0/vref+0.5);
    if (code<=0.0) { return 0U; }
    if (code>=4095.0) { return 4095U; }
    return (uint16_t)code;
}
static void handle_adc(dsp_model_t *m,const double *u)
{
    static const unsigned pin_index[DSP_ADC_CH_COUNT]={0U,1U,7U};
    uint32_t tag=(uint32_t)m->adc_channel<<16U;
    if (m->adc_phase==1U) {
        m->adc_hold=quantize_12bit(u[pin_index[m->adc_channel]],m->cfg.adc_vref_v);
        log_event(m,DSP_EVENT_ACQ_END,tag|m->adc_hold);
        m->adc_phase=2U; m->adc_event_ns=m->now_ns+m->cfg.conversion_ns; return;
    }
    m->adc_sum+=m->adc_hold; log_event(m,DSP_EVENT_EOC,tag|m->adc_hold);
    m->adc_sample_index++;
    if (m->adc_sample_index<m->cfg.oversampling) { begin_acquisition(m); return; }
    m->adc_result[m->adc_channel]=(uint16_t)((m->adc_sum+m->cfg.oversampling/2U)/m->cfg.oversampling);
    log_event(m,DSP_EVENT_ADC_RESULT,tag|m->adc_result[m->adc_channel]);
    m->adc_channel++;
    if (m->adc_channel<DSP_ADC_CH_COUNT) {
        m->adc_sum=0U; m->adc_sample_index=0U; begin_acquisition(m); return;
    }
    m->adc_phase=0U; m->adc_event_ns=DSP_TIME_NEVER;
    log_event(m,DSP_EVENT_ADC_INTERRUPT,0U);
    if (m->isr_pending) {
        m->isr_overrun_count++; log_event(m,DSP_EVENT_ISR_OVERRUN,m->isr_overrun_count); return;
    }
    memcpy(m->pending_adc,m->adc_result,sizeof(m->pending_adc));
    m->isr_pending=true; m->isr_ready_ns=m->now_ns+m->cfg.isr_latency_ns;
}
static void handle_isr_entry(dsp_model_t *m,const double *u)
{
    m->isr_pending=false; m->isr_running=true; m->isr_count++;
    m->isr_ready_ns=DSP_TIME_NEVER;
    log_event(m,DSP_EVENT_ISR_ENTRY,m->pending_adc[0]);
    if (m->cfg.control_enabled) {
        double duty=pfc_control_step(&m->control,m->pending_adc,
            m->enable_requested && !m->trip_latched && !m->comparator_high);
        m->staged_cmpa=(uint16_t)floor(duty*((double)m->cfg.tbprd+1.0)+0.5);
        if (m->staged_cmpa>m->cfg.tbprd) { m->staged_cmpa=m->cfg.tbprd; }
    } else { m->staged_cmpa=(uint16_t)u[5]; }
    log_event(m,DSP_EVENT_CONTROL,m->staged_cmpa);
    m->isr_finish_ns=m->now_ns+m->cfg.isr_execution_ns;
}
static void handle_isr_exit(dsp_model_t *m)
{
    m->isr_running=false; m->isr_finish_ns=DSP_TIME_NEVER;
    m->cmpa_shadow=(m->cfg.control_enabled && (m->trip_latched || !m->enable_requested))?0U:m->staged_cmpa;
    m->cmpa_shadow_pending=true;
    log_event(m,DSP_EVENT_SHADOW_WRITE,m->cmpa_shadow);
    log_event(m,DSP_EVENT_ISR_EXIT,0U);
}
static void handle_zero(dsp_model_t *m)
{
    double period=((double)m->cfg.tbprd+1.0)*m->cfg.tbclk_ns;
    m->last_zero_ns=m->next_zero_ns; m->next_zero_ns+=period;
    m->next_prd_ns=m->last_zero_ns+(double)m->cfg.tbprd*m->cfg.tbclk_ns;
    log_event(m,DSP_EVENT_ZERO,0U);
    if (m->cmpa_shadow_pending) {
        m->cmpa_active=m->cmpa_shadow; m->cmpa_shadow_pending=false;
        log_event(m,DSP_EVENT_SHADOW_LOAD,m->cmpa_active);
    }
    if (m->trip_clear_requested && !m->comparator_high && m->enable_requested) {
        m->trip_latched=false; m->trip_clear_requested=false;
        log_event(m,DSP_EVENT_TRIP_CLEAR,0U);
    }
    m->pwm_enabled=m->enable_requested;
    m->aq_high=m->cmpa_active>0U; m->next_cmpa_ns=DSP_TIME_NEVER;
    /* CMPA clear wins at ZERO when CMPA=0: no zero-width pulse. */
    if (m->cmpa_active==0U) { log_event(m,DSP_EVENT_CMPA,0U); }
    else { m->next_cmpa_ns=m->last_zero_ns+(double)m->cmpa_active*m->cfg.tbclk_ns; }
    refresh_pwm(m); issue_soca(m);
}
int dsp_init(dsp_model_t *m,const dsp_config_t *cfg)
{
    if (m==NULL || cfg==NULL || cfg->tbclk_ns==0U || cfg->tbprd<2U || cfg->initial_cmpa>cfg->tbprd) { return -1; }
    if (cfg->adc_enabled && (cfg->acq_ns==0U || cfg->conversion_ns==0U ||
        cfg->oversampling<1U || cfg->oversampling>16U || cfg->isr_execution_ns==0U ||
        !isfinite(cfg->adc_vref_v) || cfg->adc_vref_v<=0.0)) { return -1; }
    memset(m,0,sizeof(*m)); m->cfg=*cfg; m->now_ns=-1.0;
    m->cmpa_active=cfg->initial_cmpa; m->cmpa_shadow=cfg->initial_cmpa;
    m->next_cmpa_ns=DSP_TIME_NEVER; m->next_prd_ns=DSP_TIME_NEVER;
    m->adc_event_ns=DSP_TIME_NEVER; m->isr_ready_ns=DSP_TIME_NEVER; m->isr_finish_ns=DSP_TIME_NEVER;
    if (cfg->control_enabled && (!cfg->adc_enabled || pfc_control_init(&m->control,&cfg->control)!=0)) { return -1; }
    return 0;
}
double dsp_next_event_ns(const dsp_model_t *m)
{
    double next=earliest(m->next_zero_ns,earliest(m->next_cmpa_ns,m->next_prd_ns));
    next=earliest(next,m->adc_event_ns);
    if (m->isr_running) { next=earliest(next,m->isr_finish_ns); }
    else if (m->isr_pending) { next=earliest(next,fmax(m->now_ns,m->isr_ready_ns)); }
    return next;
}
int dsp_step(dsp_model_t *m,double t,const double u[DSP_INPUT_WIDTH])
{
    unsigned i;
    if (m==NULL || u==NULL || !isfinite(t) || t<0.0 || t>1.0e12 ||
        t<m->now_ns-EPS_NS || t>dsp_next_event_ns(m)+EPS_NS) { return -1; }
    for (i=0U;i<DSP_INPUT_WIDTH;i++) { if (!isfinite(u[i])) { return -1; } }
    if ((u[3]!=0.0 && u[3]!=1.0) || (u[4]!=0.0 && u[4]!=1.0) ||
        u[5]<0.0 || u[5]>m->cfg.tbprd || floor(u[5])!=u[5] ||
        u[6]<0.0 || u[6]>4294967295.0 || floor(u[6])!=u[6]) { return -1; }
    m->now_ns=t; m->event_size=0U; m->comparator_high=u[2]>=1.65;
    m->enable_requested=u[3]!=0.0;
    if (m->comparator_high) {
        m->trip_clear_requested=false;
        if (!m->trip_latched) { m->trip_latched=true; log_event(m,DSP_EVENT_TRIP,1U); }
    }
    if (!m->enable_requested) { m->pwm_enabled=false; }
    if (m->cfg.control_enabled && (m->trip_latched || !m->enable_requested)) {
        pfc_control_reset(&m->control);m->staged_cmpa=0U;
        m->cmpa_shadow=0U;m->cmpa_shadow_pending=true;
    }
    refresh_pwm(m);
    if (u[4]!=0.0 && !m->clear_previous && !m->comparator_high && m->enable_requested) { m->trip_clear_requested=true; }
    m->clear_previous=u[4]!=0.0;
    if (due(t,m->next_zero_ns)) { handle_zero(m); }
    if (due(t,m->next_cmpa_ns)) {
        m->next_cmpa_ns=DSP_TIME_NEVER; m->aq_high=false;
        log_event(m,DSP_EVENT_CMPA,m->cmpa_active); refresh_pwm(m);
    }
    if (due(t,m->next_prd_ns)) { m->next_prd_ns=DSP_TIME_NEVER; log_event(m,DSP_EVENT_PRD,m->cfg.tbprd); }
    if (due(t,m->adc_event_ns)) { handle_adc(m,u); }
    if (m->isr_running && due(t,m->isr_finish_ns)) { handle_isr_exit(m); }
    if (m->isr_pending && !m->isr_running && t>=m->isr_ready_ns) { handle_isr_entry(m,u); }
    if (!m->cfg.adc_enabled && (!m->command_seen || (uint32_t)u[6]!=m->command_seq)) {
        m->command_seen=true; m->command_seq=(uint32_t)u[6];
        m->cmpa_shadow=(uint16_t)u[5]; m->cmpa_shadow_pending=true;
        log_event(m,DSP_EVENT_SHADOW_WRITE,m->cmpa_shadow);
    }
    return 0;
}
void dsp_outputs(const dsp_model_t *m,double t,double y[DSP_OUTPUT_WIDTH])
{
    unsigned i; for (i=0U;i<DSP_OUTPUT_WIDTH;i++) { y[i]=0.0; }
    y[0]=m->pwm_out?1.0:0.0;
    y[1]=t<0.0?0.0:floor(fmax(0.0,t-m->last_zero_ns)/m->cfg.tbclk_ns);
    y[2]=m->cmpa_active; y[3]=m->cmpa_shadow; y[4]=m->trip_latched?1.0:0.0;
    y[5]=m->soc_count; y[6]=m->event_count;
    y[7]=m->adc_result[0]; y[8]=m->adc_result[1]; y[9]=m->adc_result[2];
    y[10]=m->isr_running?1.0:0.0; y[11]=m->isr_count;
    y[12]=m->adc_overrun_count; y[13]=m->isr_overrun_count;
    y[14]=m->isr_pending?1.0:0.0; y[15]=m->adc_phase;
    y[16]=m->control.il_a; y[17]=m->control.vbus_v; y[18]=m->control.vin_v;
    y[19]=m->control.current_ref_a; y[20]=m->control.power_w; y[21]=m->control.duty;
}
