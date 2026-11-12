function p=pfc_parameters()
% User-confirmed Issue #1 specification; all other values are teaching settings.
p.vrms=220;p.line_hz=50;p.target_vbus=400;p.power_w=3300;
p.inductance=220e-6;p.capacitance=1200e-6;
p.tbclk_ns=10;p.tbprd=1514;p.period=(p.tbprd+1)*p.tbclk_ns*1e-9;
p.voltage_gain=3.3/480;p.current_sense_v_per_a=0.05;p.current_gain=1;
p.current_bandwidth_hz=500e3;p.current_tau=1/(2*pi*p.current_bandwidth_hz);
p.current_offset_v=0;p.comparator_error_v=1e-3;p.comparator_delay_s=30e-9;
p.comparator_threshold_v=2.0; % Teaching trip: 40A nominal, no hardware claim.
p.comparator_high_v=3.3;p.adc_vref=3.3;
p.acqps=7;p.adc_clock_ns=10;p.acq_ns=(p.acqps+1)*p.adc_clock_ns;
p.conversion_ns=250;p.oversampling=1;p.isr_latency_ns=120;p.isr_execution_ns=800;
p.current_kp=0.008;p.current_ki=20;p.voltage_kp=20;p.voltage_ki=200;
p.voltage_filter_hz=20;p.min_duty=0.02;p.max_duty=0.97;
p.initial_vbus=400; % Precharged virtual bus; cold start is outside this demo.
p.load_step_time=0.5;p.load_after_w=2475;p.duration=1.0;
p.max_step=5e-6;p.rel_tol=2e-5;p.abs_tol=1e-7;
end
