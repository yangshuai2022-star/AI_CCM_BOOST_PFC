#define S_FUNCTION_NAME dsp_sfun
#define S_FUNCTION_LEVEL 2
#include "simstruc.h"
#include "dsp_behavior.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
/* Normal mode only: major Outputs previews a stack copy; Update commits.
 * Minor outputs hold gate mode; ZC is pure. Log committed events only. */
static double model_time_ns(SimStruct *S,const dsp_model_t *m)
{
    double next=dsp_next_event_ns(m);
    /* Exact scheduled hit only: never consume a timer at an earlier ZC. */
    return ssGetT(S)==next*1e-9 ? next : ssGetT(S)*1e9;
}
static void mdlInitializeSizes(SimStruct *S)
{
    ssSetNumSFcnParams(S,2);
    if (ssGetNumSFcnParams(S)!=ssGetSFcnParamsCount(S)) { return; }
    ssSetSFcnParamTunable(S,0,0); ssSetSFcnParamTunable(S,1,0);
    if (!mxIsDouble(ssGetSFcnParam(S,0)) || mxIsComplex(ssGetSFcnParam(S,0)) ||
        (mxGetNumberOfElements(ssGetSFcnParam(S,0))!=3 && mxGetNumberOfElements(ssGetSFcnParam(S,0))!=10 &&
         mxGetNumberOfElements(ssGetSFcnParam(S,0))!=24) ||
        !mxIsChar(ssGetSFcnParam(S,1))) {
        ssSetErrorStatus(S,"Expected config[3,10,24] and log filename."); return;
    }
    ssSetNumContStates(S,0); ssSetNumDiscStates(S,1);
    if (!ssSetNumInputPorts(S,1)) { return; }
    ssSetInputPortWidth(S,0,DSP_INPUT_WIDTH); ssSetInputPortRequiredContiguous(S,0,1); ssSetInputPortDirectFeedThrough(S,0,1);
    if (!ssSetNumOutputPorts(S,1)) { return; }
    ssSetOutputPortWidth(S,0,DSP_OUTPUT_WIDTH); ssSetNumSampleTimes(S,2);
    if (!ssSetNumDWork(S,1)) { return; }
    ssSetDWorkWidth(S,0,(int_T)sizeof(dsp_model_t)); ssSetDWorkDataType(S,0,SS_UINT8);
    ssSetDWorkName(S,0,"digital_state"); ssSetDWorkUsedAsDState(S,0,1);
    ssSetNumPWork(S,1); ssSetNumModes(S,1); ssSetNumNonsampledZCs(S,1);
    ssSetOperatingPointCompliance(S,DISALLOW_OPERATING_POINT);
}
static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S,0,CONTINUOUS_SAMPLE_TIME); ssSetOffsetTime(S,0,0.0);
    ssSetSampleTime(S,1,VARIABLE_SAMPLE_TIME); ssSetOffsetTime(S,1,0.0);
}
#define MDL_GET_TIME_OF_NEXT_VAR_HIT
static void mdlGetTimeOfNextVarHit(SimStruct *S)
{
    dsp_model_t m; memcpy(&m,ssGetDWork(S,0),sizeof(m));
    /* Engine queries before Update: predict from the same local transition. */
    if (dsp_step(&m,model_time_ns(S,&m),ssGetInputPortRealSignal(S,0))!=0) {
        ssSetErrorStatus(S,"Cannot schedule next DSP deadline."); return;
    }
    ssSetTNext(S,dsp_next_event_ns(&m)*1e-9);
}
#define MDL_START
static void mdlStart(SimStruct *S)
{
    char name[1024]; FILE *log;
    if (mxGetString(ssGetSFcnParam(S,1),name,sizeof(name))!=0) { ssSetErrorStatus(S,"Log path too long."); return; }
    log=fopen(name,"wb"); ssSetPWorkValue(S,0,log);
    if (log==NULL) { ssSetErrorStatus(S,"Cannot open event log."); return; }
    fprintf(log,"time_s,kind,data,pwm,active,shadow,trip\n");
}
#define MDL_INITIALIZE_CONDITIONS
static void mdlInitializeConditions(SimStruct *S)
{
    dsp_model_t m; dsp_config_t cfg; const double *p=mxGetPr(ssGetSFcnParam(S,0));
    memset(&cfg,0,sizeof(cfg));
    if (!mxIsFinite(p[0]) || p[0]<1 || p[0]>1000000 || floor(p[0])!=p[0] ||
        !mxIsFinite(p[1]) || p[1]<2 || p[1]>65535 || floor(p[1])!=p[1] ||
        !mxIsFinite(p[2]) || p[2]<0 || p[2]>p[1] || floor(p[2])!=p[2]) { ssSetErrorStatus(S,"Invalid PWM configuration."); return; }
    cfg.tbclk_ns=(uint32_t)p[0]; cfg.tbprd=(uint16_t)p[1]; cfg.initial_cmpa=(uint16_t)p[2];
    if (mxGetNumberOfElements(ssGetSFcnParam(S,0))>=10) {
        unsigned j;
        for (j=3U;j<9U;j++) {
            if (!mxIsFinite(p[j]) || p[j]<0 || p[j]>1000000000 || floor(p[j])!=p[j]) {
                ssSetErrorStatus(S,"ADC/ISR time parameters require nonnegative integer ns."); return;
            }
        }
        if (p[3]>1 || p[6]<1 || p[6]>16) { ssSetErrorStatus(S,"Invalid ADC enable/oversampling."); return; }
        cfg.adc_enabled=p[3]!=0; cfg.acq_ns=(uint32_t)p[4]; cfg.conversion_ns=(uint32_t)p[5];
        cfg.oversampling=(uint8_t)p[6]; cfg.isr_latency_ns=(uint32_t)p[7];
        cfg.isr_execution_ns=(uint32_t)p[8]; cfg.adc_vref_v=p[9];
    }
    if (mxGetNumberOfElements(ssGetSFcnParam(S,0))==24) {
        if (p[10]!=0.0 && p[10]!=1.0) { ssSetErrorStatus(S,"Invalid control enable."); return; }
        cfg.control_enabled=p[10]!=0.0;
        cfg.control.ts_s=((double)cfg.tbprd+1.0)*cfg.tbclk_ns*1e-9;
        cfg.control.current_kp=p[11];cfg.control.current_ki=p[12];
        cfg.control.voltage_kp=p[13];cfg.control.voltage_ki=p[14];
        cfg.control.amp_per_count=p[15];cfg.control.volt_per_count=p[16];
        cfg.control.vrms_v=p[17];cfg.control.target_vbus_v=p[18];
        cfg.control.initial_power_w=p[19];cfg.control.max_power_w=p[20];
        cfg.control.min_duty=p[21];cfg.control.max_duty=p[22];cfg.control.voltage_filter_hz=p[23];
    }
    if (dsp_init(&m,&cfg)!=0) { ssSetErrorStatus(S,"Invalid DSP configuration."); return; }
    memcpy(ssGetDWork(S,0),&m,sizeof(m)); ssGetRealDiscStates(S)[0]=-1.0; ssGetModeVector(S)[0]=0;
}
static void mdlOutputs(SimStruct *S,int_T tid)
{
    dsp_model_t m; double y[DSP_OUTPUT_WIDTH]; (void)tid;
    memcpy(&m,ssGetDWork(S,0),sizeof(m));
    if (ssIsMajorTimeStep(S)) { (void)dsp_step(&m,model_time_ns(S,&m),ssGetInputPortRealSignal(S,0)); }
    dsp_outputs(&m,ssIsMajorTimeStep(S)?m.now_ns:ssGetT(S)*1.0e9,y);
    if (ssIsMajorTimeStep(S) && ssGetModeVector(S)[0]!=(int_T)y[0]) { ssGetModeVector(S)[0]=(int_T)y[0]; ssSetSolverNeedsReset(S); }
    y[0]=ssGetModeVector(S)[0]; memcpy(ssGetOutputPortRealSignal(S,0),y,sizeof(y));
}
#define MDL_UPDATE
static void mdlUpdate(SimStruct *S,int_T tid)
{
    dsp_model_t m; unsigned i; FILE *log=(FILE *)ssGetPWorkValue(S,0); (void)tid;
    memcpy(&m,ssGetDWork(S,0),sizeof(m));
    if (dsp_step(&m,model_time_ns(S,&m),ssGetInputPortRealSignal(S,0))!=0) {
        ssSetErrorStatus(S,"Invalid input or missed DSP deadline; variable-step ZC required."); return;
    }
    for (i=0U;i<m.event_size;i++) {
        const dsp_event_t *e=&m.events[i];
        fprintf(log,"%.17g,%u,%u,%u,%u,%u,%u\n",e->time_ns*1e-9,(unsigned)e->kind,(unsigned)e->data,
            m.pwm_out?1U:0U,m.cmpa_active,m.cmpa_shadow,m.trip_latched?1U:0U);
    }
    memcpy(ssGetDWork(S,0),&m,sizeof(m)); ssGetRealDiscStates(S)[0]=m.now_ns;
}
#define MDL_ZERO_CROSSINGS
static void mdlZeroCrossings(SimStruct *S)
{
    ssGetNonsampledZCs(S)[0]=ssGetInputPortRealSignal(S,0)[2]-1.65;
}
static void mdlTerminate(SimStruct *S)
{
    FILE *log=(FILE *)ssGetPWorkValue(S,0); if (log!=NULL) { fclose(log); ssSetPWorkValue(S,0,NULL); }
}
#ifdef MATLAB_MEX_FILE
#include "simulink.c"
#else
#error Normal host simulation only
#endif
