#include "pfc_control.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
static double clamp(double v,double lo,double hi) { return fmin(hi,fmax(lo,v)); }
/* Conditional integration uses current saturation direction. This is a
 * separate forward-Euler variant, not the legacy previous-cycle PI in DCL.
 * Feedforward participates in anti-windup at the final output limits. */
static double update_pi(double error,double feedforward,double kp,double ki,
                        double ts,double lo,double hi,double *integral)
{
    double candidate=*integral+ki*ts*error;
    double output=feedforward+kp*error+candidate;
    if ((output<=hi && output>=lo) || (output>hi && error<0.0) || (output<lo && error>0.0)) {
        *integral=candidate;
    }
    *integral=clamp(*integral,lo-feedforward,hi-feedforward);
    return clamp(feedforward+kp*error+*integral,lo,hi);
}
void pfc_control_reset(pfc_control_t *s)
{
    pfc_config_t cfg=s->cfg;
    memset(s,0,sizeof(*s));s->cfg=cfg;s->filtered_vbus_v=cfg.target_vbus_v;
}
int pfc_control_init(pfc_control_t *s,const pfc_config_t *cfg)
{
    if (s==NULL || cfg==NULL || !isfinite(cfg->ts_s) || cfg->ts_s<=0.0 ||
        !isfinite(cfg->amp_per_count) || cfg->amp_per_count<=0.0 ||
        !isfinite(cfg->volt_per_count) || cfg->volt_per_count<=0.0 ||
        !isfinite(cfg->vrms_v) || cfg->vrms_v<=0.0 ||
        !isfinite(cfg->target_vbus_v) || cfg->target_vbus_v<=0.0 ||
        !isfinite(cfg->initial_power_w) || cfg->initial_power_w<0.0 ||
        !isfinite(cfg->max_power_w) || cfg->max_power_w<=cfg->initial_power_w ||
        !isfinite(cfg->current_kp) || cfg->current_kp<0.0 ||
        !isfinite(cfg->current_ki) || cfg->current_ki<0.0 ||
        !isfinite(cfg->voltage_kp) || cfg->voltage_kp<0.0 ||
        !isfinite(cfg->voltage_ki) || cfg->voltage_ki<0.0 ||
        !isfinite(cfg->voltage_filter_hz) || cfg->voltage_filter_hz<=0.0 ||
        !isfinite(cfg->min_duty) || !isfinite(cfg->max_duty) ||
        cfg->min_duty<0.0 || cfg->min_duty>=cfg->max_duty || cfg->max_duty>=1.0) { return -1; }
    memset(s,0,sizeof(*s));s->cfg=*cfg;
    s->power_integral=cfg->initial_power_w;s->filtered_vbus_v=cfg->target_vbus_v;
    return 0;
}
double pfc_control_step(pfc_control_t *s,const uint16_t adc[3],bool permit)
{
    const pfc_config_t *c=&s->cfg;
    double alpha,feedforward;
    if (!permit || adc[0]>4095U || adc[1]>4095U || adc[2]>4095U) {
        pfc_control_reset(s);return 0.0;
    }
    s->il_a=adc[0]*c->amp_per_count;
    s->vbus_v=adc[1]*c->volt_per_count;s->vin_v=adc[2]*c->volt_per_count;
    if (s->vbus_v<50.0 || s->vbus_v>=479.8) { pfc_control_reset(s);return 0.0; }
    alpha=c->ts_s/(c->ts_s+1.0/(6.283185307179586*c->voltage_filter_hz));
    s->filtered_vbus_v+=alpha*(s->vbus_v-s->filtered_vbus_v);
    s->power_w=update_pi(c->target_vbus_v-s->filtered_vbus_v,0.0,
        c->voltage_kp,c->voltage_ki,c->ts_s,0.0,c->max_power_w,&s->power_integral);
    s->current_ref_a=s->power_w*s->vin_v/(c->vrms_v*c->vrms_v);
    feedforward=clamp(1.0-s->vin_v/s->vbus_v,0.0,c->max_duty);
    s->duty=update_pi(s->current_ref_a-s->il_a,feedforward,
        c->current_kp,c->current_ki,c->ts_s,c->min_duty,c->max_duty,&s->current_integral);
    return s->duty;
}
