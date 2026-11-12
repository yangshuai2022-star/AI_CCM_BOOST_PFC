#ifndef PFC_CONTROL_H
#define PFC_CONTROL_H
#include <stdbool.h>
#include <stdint.h>
/* Independent host C99 AVG controller. Owner: ISR entry, once/PWM period.
 * double math for solver comparison; no MCU cycle/FPU portability claim.
 * Parameters: A/count, V/count, seconds, V, W, duty/A, duty/(A*s),
 * W/V, W/(V*s), Hz, duty limits. No registers, plant or analog filter. */
typedef struct {
    double ts_s, amp_per_count, volt_per_count, vrms_v, target_vbus_v;
    double initial_power_w, max_power_w, current_kp, current_ki;
    double voltage_kp, voltage_ki, voltage_filter_hz, min_duty, max_duty;
} pfc_config_t;
typedef struct {
    pfc_config_t cfg;
    double current_integral, power_integral, filtered_vbus_v;
    double il_a, vbus_v, vin_v, current_ref_a, power_w, duty;
} pfc_control_t;
int pfc_control_init(pfc_control_t *s,const pfc_config_t *cfg);
/* Fault/disable resets all integrators, telemetry and pending duty to zero;
 * restart uses zero stored power (soft integration), initial precharge only
 * is seeded with cfg.initial_power_w. Invalid ADC/measurements return 0 duty. */
void pfc_control_reset(pfc_control_t *s);
double pfc_control_step(pfc_control_t *s,const uint16_t adc[3],bool permit);
#endif
