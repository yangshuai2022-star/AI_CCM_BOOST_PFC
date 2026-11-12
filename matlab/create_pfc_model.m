function model=create_pfc_model()
root=fileparts(fileparts(mfilename('fullpath')));model='ccm_boost_pfc';
file=fullfile(root,'matlab',[model '.slx']);
if bdIsLoaded(model),close_system(model,0);end
if isfile(file),load_system(file);upgrade_frontend(model);return;end
new_system(model);load_system('simulink');
set_param(model,'Solver','ode23tb','SolverType','Variable-step','MaxStep','p.max_step',...
 'RelTol','p.rel_tol','AbsTol','p.abs_tol','ZeroCrossControl','EnableAll',...
 'ZeroCrossAlgorithm','Adaptive','StopTime','p.duration','SaveTime','on',...
 'TimeSaveName','tout','ReturnWorkspaceOutputs','on','Refine','1');
add_block('simulink/Sources/Sine Wave',[model '/AC_220Vrms_50Hz'],...
 'Amplitude','p.vrms*sqrt(2)','Frequency','2*pi*p.line_hz','Position',[35 30 125 65]);
add_block('simulink/Math Operations/Abs',[model '/Ideal_rectifier'],'Position',[165 30 200 65]);
add_line(model,'AC_220Vrms_50Hz/1','Ideal_rectifier/1');
add_block('simulink/Sources/Step',[model '/Load_conductance'],...
 'Time','p.load_step_time','Before','p.power_w/p.target_vbus^2','After','p.load_after_w/p.target_vbus^2',...
 'Position',[35 115 145 150]);
add_block('simulink/Continuous/Integrator',[model '/Inductor_current_A'],...
 'InitialCondition','0','LimitOutput','on','LowerSaturationLimit','0','UpperSaturationLimit','inf',...
 'Position',[285 185 320 220]);
add_block('simulink/Continuous/Integrator',[model '/Bus_voltage_V'],...
 'InitialCondition','p.initial_vbus','LimitOutput','on','LowerSaturationLimit','0','UpperSaturationLimit','inf',...
 'Position',[285 270 320 305]);
add_block('simulink/Signal Routing/Mux',[model '/Power_stage_signals'],'Inputs','5','Position',[35 260 40 410]);
add_block('simulink/User-Defined Functions/Fcn',[model '/Boost_L_equation'],...
 'Expr','(u(1)-(1-u(4))*u(2))/p.inductance','Position',[95 190 240 225]);
add_block('simulink/User-Defined Functions/Fcn',[model '/Bus_C_equation'],...
 'Expr','((1-u(4))*u(3)-u(5)*u(2))/p.capacitance','Position',[95 275 240 310]);
add_line(model,'Ideal_rectifier/1','Power_stage_signals/1','autorouting','on');
add_line(model,'Bus_voltage_V/1','Power_stage_signals/2','autorouting','on');
add_line(model,'Inductor_current_A/1','Power_stage_signals/3','autorouting','on');
add_line(model,'Load_conductance/1','Power_stage_signals/5','autorouting','on');
add_line(model,'Power_stage_signals/1','Boost_L_equation/1','autorouting','on');
add_line(model,'Power_stage_signals/1','Bus_C_equation/1','autorouting','on');
add_line(model,'Boost_L_equation/1','Inductor_current_A/1');add_line(model,'Bus_C_equation/1','Bus_voltage_V/1');
add_analog_frontend(model);connect_dsp(model);
add_line(model,'Inductor_current_A/1','Analog_frontend/1','autorouting','on');
add_line(model,'Bus_voltage_V/1','Analog_frontend/2','autorouting','on');
add_line(model,'Ideal_rectifier/1','Analog_frontend/3','autorouting','on');
add_block('simulink/Signal Routing/Selector',[model '/PWM_gate'],'NumberOfDimensions','1',...
 'InputPortWidth','22','Indices','1','Position',[1070 160 1135 195]);
add_line(model,'Digital_peripherals/1','PWM_gate/1');
add_line(model,'PWM_gate/1','Power_stage_signals/4','autorouting','on');
add_block('simulink/Signal Routing/Mux',[model '/Physical_signals'],'Inputs','5','Position',[370 35 375 150]);
signals={'AC_220Vrms_50Hz','Ideal_rectifier','Inductor_current_A','Bus_voltage_V','Load_conductance'};
for j=1:5,add_line(model,[signals{j} '/1'],sprintf('Physical_signals/%d',j),'autorouting','on');end
add_block('simulink/Sinks/To Workspace',[model '/Physical_trace'],'VariableName','physical_trace',...
 'SaveFormat','Structure With Time','Position',[420 35 535 70]);
add_line(model,'Physical_signals/1','Physical_trace/1');
set_param(model,'ZoomFactor','FitSystem');save_system(model,file);
end
