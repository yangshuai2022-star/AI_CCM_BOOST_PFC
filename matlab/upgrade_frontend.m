function upgrade_frontend(model)
% Keep the amplifier state in volts so AbsTol has an explicit physical unit.
b=[model '/Analog_frontend/Amplifier_500kHz'];
if strcmp(get_param(b,'BlockType'),'TransferFcn')
 load_system('simulink');
 replace_block([model '/Analog_frontend'],'BlockType','TransferFcn','simulink/Continuous/State-Space','noprompt');
 set_param(b,'A','-1/p.current_tau','B','1/p.current_tau','C','1','D','0','X0','0');
 save_system(model);
end
end
