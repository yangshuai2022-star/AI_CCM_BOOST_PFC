function validate_control_reset()
p=pfc_parameters();c=dsp_config(p,1);T=p.period;
b=[0.5 2.75 0 1 0 1000 0 2];high=b;high(3)=3.3;clear=b;clear(5)=1;
s=[0 b;1500.123e-9 high;2e-6 b;5e-6 clear;6e-6 b;70e-6 b];
[e,o,~]=event_case('control_reset',s,c,45e-6,7.3e-6,'M3');
assert(any(e.kind==17 & abs(e.time_s-1110e-9)<1e-12 & e.data>0));
assert(any(e.kind==9 & abs(e.time_s-1910e-9)<1e-12 & e.data==0));
assert(any(e.kind==1 & abs(e.time_s-T)<1e-12 & e.data==0));
assert(~any(e.kind==10 & e.data==1 & e.time_s>1500.123e-9 & e.time_s<2*T-1e-12));
t=o.trace.time;y=o.trace.signals.values;mask=t>=1500.123e-9 & t<=T;
assert(all(y(mask,22)==0) && all(y(mask,21)==0));
fprintf('Control reset PASS: Trip during running ISR discards precomputed CMPA, exit writes zero; explicit rearm cannot load stale duty.\n');
close_system('dsp_event_test',0);
end
