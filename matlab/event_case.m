function [e,out,elapsed]=event_case(name,stimulus,config,stop,maxstep,stage)
if nargin<6, stage='M1'; end
root=fileparts(fileparts(mfilename('fullpath'))); folder=fullfile(root,'evidence',stage);
if ~isfolder(folder), mkdir(folder); end
model='dsp_event_test'; event_file=fullfile(folder,[name '_events.csv']);
input=Simulink.SimulationInput(model);
input=input.setVariable('stimulus',step_stimulus(stimulus)).setVariable('config',config).setVariable('event_file',event_file);
input=input.setBlockParameter([model '/Stimulus'],'Interpolate','on');
input=input.setBlockParameter([model '/Stimulus'],'ZeroCross','on');
input=input.setModelParameter('ZCThreshold','1e-12');
input=input.setModelParameter('StopTime',num2str(stop,17),'MaxStep',num2str(maxstep,17));
tic; out=sim(input); elapsed=toc;
e=readtable(event_file); save(fullfile(folder,[name '_trace.mat']),'out','elapsed');
end
