function probe_closed_loop()
addpath(fileparts(mfilename('fullpath')));build_mex();create_pfc_model();
p=pfc_parameters();p.duration=0.08;p.load_step_time=1;
[out,elapsed]=pfc_case('pfc_probe',p,'ccm_boost_pfc');
y=out.physical_trace.signals.values;t=out.physical_trace.time;
fprintf('probe last bus %.3f, mean last20ms %.3f, max IL %.3f, wall %.3f\n',y(end,4),mean(y(t>0.06,4)),max(y(:,3)),elapsed);
close_system('ccm_boost_pfc',0);
end
