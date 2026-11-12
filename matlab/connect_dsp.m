function connect_dsp(model)
% Frontend vector [IL pin,VBUS pin,VIN pin,OC logic,raw OC] -> digital port.
add_block('simulink/Signal Routing/Demux',[model '/Analog_pins'],'Outputs','5','Position',[680 170 685 330]);
add_line(model,'Analog_frontend/1','Analog_pins/1');
add_block('simulink/Signal Routing/Mux',[model '/Digital_inputs'],'Inputs','8','Position',[790 160 795 370]);
add_line(model,'Analog_pins/1','Digital_inputs/1');add_line(model,'Analog_pins/2','Digital_inputs/2');
add_line(model,'Analog_pins/4','Digital_inputs/3');add_line(model,'Analog_pins/3','Digital_inputs/8');
names={'Safe_enable','Clear_request','Fixture_CMPA','Write_sequence'};values={'1','0','1000','0'};
for j=1:4
 add_block('simulink/Sources/Constant',[model '/' names{j}],'Value',values{j},'Position',[695 370+45*j 755 395+45*j]);
 add_line(model,[names{j} '/1'],sprintf('Digital_inputs/%d',j+3),'autorouting','on');
end
add_block('simulink/Sinks/Terminator',[model '/Raw_comparator_sink'],'Position',[740 310 760 330]);
add_line(model,'Analog_pins/5','Raw_comparator_sink/1');
add_block('simulink/User-Defined Functions/S-Function',[model '/Digital_peripherals'],...
 'FunctionName','dsp_sfun','Parameters','config,event_file','Position',[850 180 1010 270]);
add_line(model,'Digital_inputs/1','Digital_peripherals/1');
add_block('simulink/Sinks/To Workspace',[model '/Digital_trace'],'VariableName','digital_trace',...
 'SaveFormat','Structure With Time','Position',[1080 250 1190 285]);
add_line(model,'Digital_peripherals/1','Digital_trace/1');
add_block('simulink/Sinks/To Workspace',[model '/Analog_trace'],'VariableName','analog_trace',...
 'SaveFormat','Structure With Time','Position',[680 65 790 100]);
add_line(model,'Analog_frontend/1','Analog_trace/1');
end
