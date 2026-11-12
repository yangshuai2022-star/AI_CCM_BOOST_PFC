function validate_efficiency()
% Short, isolated reference only. Never run the full plant on a 10ns grid.
root=fileparts(fileparts(mfilename('fullpath')));folder=fullfile(root,'evidence','M3');
create_event_model();b=[0 0 0 1 0 1000 0 0];
[events,eventout,eventwall]=event_case('efficiency',[0 b;100e-6 b],[10 1514 1000],60e-6,7.3e-6,'M3');
model='pwm_tick_reference';file=fullfile(root,'matlab',[model '.slx']);
if bdIsLoaded(model),close_system(model,0);end
if isfile(file),load_system(file);
else
 new_system(model);load_system('simulink');
 set_param(model,'SolverType','Fixed-step','Solver','FixedStepDiscrete','FixedStep','1e-8',...
  'StopTime','60e-6','SaveTime','on','TimeSaveName','tout','ReturnWorkspaceOutputs','on');
 add_block('simulink/Sources/Pulse Generator',[model '/PWM_tick_reference'],...
  'PulseType','Sample based','Period','1515','PulseWidth','1000','SampleTime','1e-8','Position',[40 40 160 80]);
 add_block('simulink/Sinks/To Workspace',[model '/Reference_trace'],'VariableName','trace',...
  'SaveFormat','Structure With Time','Position',[240 40 360 80]);
 add_line(model,'PWM_tick_reference/1','Reference_trace/1');save_system(model,file);
end
tic;reference=sim(model);refwall=toc;
r=reference.trace;up=r.time([true;diff(r.signals.values)>0]);down=r.time([false;diff(r.signals.values)<0]);
assert(max(abs(up(1:4)-(0:3)'*15150e-9))<1e-12);
assert(max(abs(down(1:4)-((0:3)'*15150e-9+10000e-9)))<1e-12);
assert(numel(reference.tout)==6001);assert(numel(eventout.tout)<100);
comparison=table([height(events);NaN],[numel(eventout.tout);numel(reference.tout)],...
 [eventwall;refwall],'RowNames',{'analytic_events','10ns_short_reference'},...
 'VariableNames',{'committed_events','solver_times','wall_s'});
disp(comparison);writetable(comparison,fullfile(folder,'efficiency.csv'),'WriteRowNames',true);
close_system(model,0);close_system('dsp_event_test',0);
end
