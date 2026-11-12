function [out,elapsed,event_file]=pfc_case(name,p,model,stimulus,interpolate)
root=fileparts(fileparts(mfilename('fullpath')));folder=fullfile(root,'evidence','M3');
if ~isfolder(folder),mkdir(folder);end
event_file=fullfile(folder,[name '_events.csv']);
if strcmp(model,'ccm_boost_pfc'),control=1;else,control=0;end
input=Simulink.SimulationInput(model);
input=input.setModelParameter('ZCThreshold','1e-12');
input=input.setVariable('p',p).setVariable('config',dsp_config(p,control)).setVariable('event_file',event_file);
if nargin>=4
 if strcmp(interpolate,'off'),stimulus=step_stimulus(stimulus);interpolate='on';end
 input=input.setVariable('stimulus',stimulus);
 input=input.setBlockParameter([model '/Stimulus'],'Interpolate',interpolate);
 input=input.setBlockParameter([model '/Stimulus'],'ZeroCross','on');
end
tic;out=sim(input);elapsed=toc;
save(fullfile(folder,[name '_trace.mat']),'out','elapsed','p','-v7.3');
fprintf('%s: %.6gs simulated, %d solver points, %.3fs wall\n',name,p.duration,numel(out.tout),elapsed);
end
