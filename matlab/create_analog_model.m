function model=create_analog_model()
root=fileparts(fileparts(mfilename('fullpath')));model='pfc_analog_test';
file=fullfile(root,'matlab',[model '.slx']);
if bdIsLoaded(model),close_system(model,0);end
if isfile(file),load_system(file);upgrade_frontend(model);return;end
new_system(model);load_system('simulink');
set_param(model,'Solver','ode45','SolverType','Variable-step','MaxStep','p.max_step',...
 'RelTol','1e-8','AbsTol','1e-10','ZeroCrossControl','EnableAll','ZeroCrossAlgorithm','Adaptive',...
 'StopTime','p.duration','SaveTime','on','TimeSaveName','tout','ReturnWorkspaceOutputs','on','Refine','1');
add_block('simulink/Sources/From Workspace',[model '/Stimulus'],'VariableName','stimulus',...
 'Interpolate','off','OutputAfterFinalValue','Holding final value','SampleTime','0','Position',[30 170 140 205]);
add_block('simulink/Signal Routing/Demux',[model '/Physical_inputs'],'Outputs','3','Position',[200 160 205 260]);
add_line(model,'Stimulus/1','Physical_inputs/1');
add_analog_frontend(model);connect_dsp(model);
for j=1:3,add_line(model,sprintf('Physical_inputs/%d',j),sprintf('Analog_frontend/%d',j));end
save_system(model,file);
end
