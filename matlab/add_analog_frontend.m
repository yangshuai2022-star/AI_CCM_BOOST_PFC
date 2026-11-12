function add_analog_frontend(model)
% Same independently parameterized analog subsystem in plant and calibration tests.
s=[model '/Analog_frontend'];add_block('built-in/Subsystem',s,'Position',[470 180 630 325]);
add_block('simulink/Sources/In1',[s '/Current_A'],'Port','1','Position',[20 30 50 45]);
add_block('simulink/Sources/In1',[s '/Bus_V'],'Port','2','Position',[20 210 50 225]);
add_block('simulink/Sources/In1',[s '/Rectified_input_V'],'Port','3','Position',[20 260 50 275]);
add_block('simulink/Math Operations/Gain',[s '/Shunt_and_gain'],...
 'Gain','p.current_sense_v_per_a*p.current_gain','Position',[90 20 185 55]);
add_block('simulink/Continuous/State-Space',[s '/Amplifier_500kHz'],...
 'A','-1/p.current_tau','B','1/p.current_tau','C','1','D','0','X0','0','Position',[220 20 315 55]);
add_block('simulink/Math Operations/Bias',[s '/Zero_offset'],...
 'Bias','p.current_offset_v','Position',[350 20 420 55]);
add_line(s,'Current_A/1','Shunt_and_gain/1');add_line(s,'Shunt_and_gain/1','Amplifier_500kHz/1');
add_line(s,'Amplifier_500kHz/1','Zero_offset/1');
add_block('simulink/Math Operations/Gain',[s '/DC_scaling'],'Gain','p.voltage_gain','Position',[90 200 185 235]);
add_block('simulink/Math Operations/Gain',[s '/AC_rectified_scaling'],'Gain','p.voltage_gain','Position',[90 250 185 285]);
add_line(s,'Bus_V/1','DC_scaling/1');add_line(s,'Rectified_input_V/1','AC_rectified_scaling/1');
add_block('simulink/Math Operations/Bias',[s '/Comparator_1mV_error'],...
 'Bias','p.comparator_error_v','Position',[460 20 570 55]);
add_block('simulink/Continuous/Transport Delay',[s '/Analog_propagation_30ns'],...
 'DelayTime','p.comparator_delay_s','InitialOutput','0','Position',[610 20 735 55]);
add_block('simulink/Sources/Constant',[s '/Comparator_threshold'],...
 'Value','p.comparator_threshold_v','Position',[460 110 570 140]);
add_block('simulink/Logic and Bit Operations/Relational Operator',[s '/Delayed_comparator'],...
 'Operator','>=','Position',[780 20 820 65]);
add_block('simulink/Signal Attributes/Data Type Conversion',[s '/Logic_to_double'],...
 'OutDataTypeStr','double','Position',[850 20 920 55]);
add_block('simulink/Math Operations/Gain',[s '/Logic_3V3'],...
 'Gain','p.comparator_high_v','Position',[950 20 1010 55]);
add_block('simulink/Logic and Bit Operations/Relational Operator',[s '/Raw_comparator'],...
 'Operator','>=','Position',[610 110 650 155]);
add_block('simulink/Signal Attributes/Data Type Conversion',[s '/Raw_to_double'],...
 'OutDataTypeStr','double','Position',[690 110 760 145]);
add_line(s,'Zero_offset/1','Comparator_1mV_error/1');
add_line(s,'Comparator_1mV_error/1','Analog_propagation_30ns/1');
add_line(s,'Analog_propagation_30ns/1','Delayed_comparator/1');
add_line(s,'Comparator_threshold/1','Delayed_comparator/2');
add_line(s,'Delayed_comparator/1','Logic_to_double/1');add_line(s,'Logic_to_double/1','Logic_3V3/1');
add_line(s,'Comparator_1mV_error/1','Raw_comparator/1');add_line(s,'Comparator_threshold/1','Raw_comparator/2');
add_line(s,'Raw_comparator/1','Raw_to_double/1');
add_block('simulink/Signal Routing/Mux',[s '/Analog_outputs'],'Inputs','5','Position',[1080 90 1085 270]);
signals={'Zero_offset','DC_scaling','AC_rectified_scaling','Logic_3V3','Raw_to_double'};
for j=1:5,add_line(s,[signals{j} '/1'],sprintf('Analog_outputs/%d',j),'autorouting','on');end
add_block('simulink/Sinks/Out1',[s '/Pins_and_comparator'],'Position',[1150 175 1180 190]);
add_line(s,'Analog_outputs/1','Pins_and_comparator/1');
end
