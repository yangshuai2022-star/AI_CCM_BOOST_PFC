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
    refresh_pwm(m); m->soc_count++; log_event(m,DSP_EVENT_SOCA,0U);
}
int dsp_init(dsp_model_t *m,const dsp_config_t *cfg)
{
    if (m==NULL || cfg==NULL || cfg->tbclk_ns==0U || cfg->tbprd<2U || cfg->initial_cmpa>cfg->tbprd) { return -1; }
    memset(m,0,sizeof(*m)); m->cfg=*cfg; m->now_ns=-1.0;
    m->cmpa_active=cfg->initial_cmpa; m->cmpa_shadow=cfg->initial_cmpa;
    m->next_cmpa_ns=DSP_TIME_NEVER; m->next_prd_ns=DSP_TIME_NEVER;
    return 0;
}
double dsp_next_event_ns(const dsp_model_t *m)
{
    return earliest(m->next_zero_ns,earliest(m->next_cmpa_ns,m->next_prd_ns));
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
    refresh_pwm(m);
    if (u[4]!=0.0 && !m->clear_previous && !m->comparator_high && m->enable_requested) { m->trip_clear_requested=true; }
    m->clear_previous=u[4]!=0.0;
    if (due(t,m->next_zero_ns)) { handle_zero(m); }
    if (due(t,m->next_cmpa_ns)) {
        m->next_cmpa_ns=DSP_TIME_NEVER; m->aq_high=false;
        log_event(m,DSP_EVENT_CMPA,m->cmpa_active); refresh_pwm(m);
    }
    if (due(t,m->next_prd_ns)) { m->next_prd_ns=DSP_TIME_NEVER; log_event(m,DSP_EVENT_PRD,m->cfg.tbprd); }
    if (!m->command_seen || (uint32_t)u[6]!=m->command_seq) {
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
}
