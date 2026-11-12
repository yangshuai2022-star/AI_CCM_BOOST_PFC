function model=create_event_model()
root=fileparts(fileparts(mfilename('fullpath'))); model='dsp_event_test';
if bdIsLoaded(model), close_system(model,0); end
file=fullfile(root,'matlab',[model '.slx']);
if isfile(file), load_system(file); return; end
new_system(model); load_system('simulink');
set_param(model,'Solver','ode45','SolverType','Variable-step','MaxStep','7.3e-6',...
 'RelTol','1e-7','AbsTol','1e-9','ZeroCrossControl','EnableAll','ZeroCrossAlgorithm','Adaptive',...
 'StopTime','80e-6','SaveTime','on','TimeSaveName','tout','ReturnWorkspaceOutputs','on');
add_block('simulink/Sources/From Workspace',[model '/Stimulus'],'VariableName','stimulus',...
 'Interpolate','off','OutputAfterFinalValue','Holding final value','SampleTime','0','Position',[30 90 145 130]);
add_block('simulink/User-Defined Functions/S-Function',[model '/Digital_peripherals'],...
 'FunctionName','dsp_sfun','Parameters','config,event_file','Position',[220 70 400 145]);
add_block('simulink/Sinks/To Workspace',[model '/Trace'],'VariableName','trace',...
 'SaveFormat','Structure With Time','Position',[470 85 590 125]);
add_line(model,'Stimulus/1','Digital_peripherals/1'); add_line(model,'Digital_peripherals/1','Trace/1');
add_block('simulink/Sources/Constant',[model '/One'],'Value','1','Position',[50 230 80 260]);
add_block('simulink/Continuous/Integrator',[model '/Solver_clock'],'Position',[150 230 180 260]);
add_block('simulink/Sinks/Terminator',[model '/Clock_sink'],'Position',[250 230 270 250]);
add_line(model,'One/1','Solver_clock/1'); add_line(model,'Solver_clock/1','Clock_sink/1');
save_system(model,fullfile(root,'matlab',[model '.slx']));
end
