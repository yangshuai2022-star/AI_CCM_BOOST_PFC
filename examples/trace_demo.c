#include "dsp_behavior.h"
#include <stdio.h>
int main(void)
{
    dsp_model_t m;
    dsp_config_t cfg={10U,1514U,1000U};
    double u[DSP_INPUT_WIDTH]={0,0,0,1,0,600,0,0};
    unsigned i;
    if (dsp_init(&m,&cfg)!=0) { return 1; }
    puts("time_ns,event,data,pwm_out,cmpa_active,cmpa_shadow");
    while (dsp_next_event_ns(&m)<50000.0) {
        if (dsp_step(&m,dsp_next_event_ns(&m),u)!=0) { return 2; }
        for (i=0U;i<m.event_size;i++) {
            printf("%.9f,%u,%u,%u,%u,%u\n",m.events[i].time_ns,
                (unsigned)m.events[i].kind,(unsigned)m.events[i].data,
                m.pwm_out?1U:0U,m.cmpa_active,m.cmpa_shadow);
        }
    }
    return 0;
}
