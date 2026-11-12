function validate_m3()
root=fileparts(fileparts(mfilename('fullpath')));folder=fullfile(root,'evidence','M3');
if ~isfolder(folder),mkdir(folder);end
fid=fopen(fullfile(folder,'matlab.txt'),'w');fclose(fid);diary(fullfile(folder,'matlab.txt'));
cleanup=onCleanup(@()diary('off'));disp(version);build_mex();
validate_analog();validate_efficiency();validate_control_reset();create_pfc_model();p=pfc_parameters();
fprintf('Latest specification: %.0fW, %.0fuH, %.0fuF, target 66kHz, actual %.9fHz, TBPRD=%d.\n',p.power_w,p.inductance*1e6,p.capacitance*1e6,1/p.period,p.tbprd);
nominal=p;nominal.duration=0.5;nominal.load_step_time=2;
[steady,steady_wall,steadyfile]=pfc_case('pfc_steady',nominal,'ccm_boost_pfc');
sm=pfc_metrics(steady,nominal,0.46,0.50);sm.wall_s=steady_wall;
assert(abs(sm.vbus_mean_V-400)<5 && sm.vbus_pp_V<35);
assert(sm.ADC_overrun==0 && sm.ISR_overrun==0);
assert(sm.events<0.5/10e-9/10 && sm.solver_times<0.5/10e-9/10);
assert(all(steady.digital_trace.signals.values(:,5)==0));
plot_pfc(steady,nominal,'pfc_steady');archive_events(steadyfile);
[step,step_wall,stepfile]=pfc_case('pfc_load_step',p,'ccm_boost_pfc');
before=pfc_metrics(step,p,p.load_step_time-0.06,p.load_step_time-0.02);
after=pfc_metrics(step,p,p.duration-0.04,p.duration);after.wall_s=step_wall;
assert(abs(before.vbus_mean_V-400)<5 && abs(after.vbus_mean_V-400)<5);
assert(after.ADC_overrun==0 && after.ISR_overrun==0);
assert(all(step.digital_trace.signals.values(:,5)==0));
assert(abs(before.output_power_W-after.output_power_W)>500);
response=measure_load_response(step,p);
plot_pfc(step,p,'pfc_load_step');archive_events(stepfile);
% Actual current drives the same amplifier/comparator; lower threshold is a
% test-only overcurrent setting (14A), not a claimed production threshold.
fault=p;fault.duration=0.04;fault.load_step_time=1;fault.comparator_threshold_v=0.7;
[tripped,~,tripfile]=pfc_case('pfc_trip',fault,'ccm_boost_pfc');
e=readtable(tripfile);trip_t=e.time_s(find(e.kind==11,1));assert(~isempty(trip_t));
a=tripped.analog_trace;d=tripped.digital_trace;
oc_t=a.time(find(a.signals.values(:,4)>1.65,1));assert(abs(trip_t-oc_t)<1e-12);
assert(all(d.signals.values(d.time>=trip_t,1)==0));
assert(all(d.signals.values(d.time>=trip_t,5)==1));
assert(any(a.signals.values(a.time>trip_t+1e-3,4)==0));
assert(all(d.signals.values(d.time>trip_t+50e-6,22)==0));
fault.focus_time=trip_t;plot_pfc(tripped,fault,'pfc_trip');archive_events(tripfile);
summary=struct('steady',sm,'before_step',before,'after_step',after,'load_response',response,'trip_time_s',trip_t);
fid=fopen(fullfile(folder,'summary.json'),'w');fprintf(fid,'%s\n',jsonencode(summary));fclose(fid);
fprintf('M3 PASS: analog calibration/bandwidth/30ns comparator; 3.3kW steady loop; 3300->2475W load step; physical-current Trip/latch; event efficiency.\n');
close_system('ccm_boost_pfc',0);
end
function archive_events(file)
gzip(file);
e=readtable(file);sel=e.time_s<60e-6 | (e.time_s>0.104 & e.time_s<0.104+60e-6) | e.kind==11 | e.kind==12;
[folder,name]=fileparts(file);writetable(e(sel,:),fullfile(folder,[name '_excerpt.csv']));
end
