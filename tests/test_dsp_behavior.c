#include "dsp_behavior.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    dsp_model_t m;
    dsp_config_t cfg={10U,1514U,1000U};
    double u[DSP_INPUT_WIDTH]={0,0,0,1,0,400,0,0};
    assert(dsp_init(&m,&cfg)==0);
    assert(dsp_step(&m,0.0,u)==0 && m.pwm_out);
    assert(m.cmpa_active==1000U && m.cmpa_shadow==400U);
    u[2]=3.3;
    assert(dsp_step(&m,1234.567,u)==0 && !m.pwm_out && m.trip_latched);
    u[2]=0.0;
    while (dsp_next_event_ns(&m)<15150.0) {
        assert(dsp_step(&m,dsp_next_event_ns(&m),u)==0);
    }
    assert(dsp_step(&m,15150.0,u)==0 && m.trip_latched);
    assert(m.cmpa_active==400U);
    assert(dsp_step(&m,99999.0,u)!=0);
    puts("PASS: C auxiliary smoke (not MATLAB acceptance)");
    return 0;
}
