function response=measure_load_response(out,p)
% Average each full line period to separate transient envelope from 100Hz ripple.
root=fileparts(fileparts(mfilename('fullpath')));folder=fullfile(root,'evidence','M3');
t=out.physical_trace.time;v=out.physical_trace.signals.values(:,4);
edges=(0:1/p.line_hz:p.duration)';area=cumtrapz(t,v);
means=diff(interp1(t,area,edges))./diff(edges);mid=(edges(1:end-1)+edges(2:end))/2;
post=find(mid>=p.load_step_time);bad=post(abs(means(post)-p.target_vbus)>5);
if isempty(bad),settled=edges(post(1)+1)-p.load_step_time;
elseif bad(end)==numel(means),settled=NaN;
else,settled=edges(bad(end)+2)-p.load_step_time;end
response=struct('line_cycle_mean_peak_V',max(means(post)),...
 'line_cycle_mean_min_V',min(means(post)),'recovery_to_5V_band_s',settled,...
 'time_resolution_s',1/p.line_hz,'criterion','all subsequent complete line-period means within 400 +/-5V');
writetable(table(mid,means),fullfile(folder,'load_step_envelope.csv'));
fid=fopen(fullfile(folder,'load_step_response.json'),'w');fprintf(fid,'%s\n',jsonencode(response));fclose(fid);
disp(response);
end
