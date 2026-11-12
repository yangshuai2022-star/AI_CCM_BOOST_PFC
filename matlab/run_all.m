function run_all()
% Functional acceptance exclusively in local MATLAB/Simulink.
addpath(fileparts(mfilename('fullpath')));
validate_m1();validate_m2();validate_m3();
close_system('simulink',0);
fprintf('ALL STAGES PASS in MATLAB/Simulink. See evidence/M1, M2, M3.\n');
end
