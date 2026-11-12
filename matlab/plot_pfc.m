function plot_pfc(out,p,name)
root=fileparts(fileparts(mfilename('fullpath')));folder=fullfile(root,'evidence','M3');
t=out.physical_trace.time;y=out.physical_trace.signals.values;
% Downsample display only; metrics and saved traces retain full solver data.
ix=1:max(1,floor(numel(t)/20000)):numel(t);
f=figure('Visible','off','Position',[100 100 1000 1000]);
subplot(4,1,1);plot(t(ix),y(ix,1),t(ix),sign(y(ix,1)).*y(ix,3)*10);legend('AC V','input A x10');grid on;
subplot(4,1,2);plot(t(ix),y(ix,3));ylabel('i_L (A)');grid on;
subplot(4,1,3);plot(t(ix),y(ix,4));ylabel('Bus V');grid on;
d=out.digital_trace;id=1:max(1,floor(numel(d.time)/20000)):numel(d.time);
subplot(4,1,4);plot(d.time(id),d.signals.values(id,[3 4]));legend('Active','Shadow');xlabel('time (s)');grid on;
saveas(f,fullfile(folder,[name '.png']));close(f);
f=figure('Visible','off','Position',[100 100 1000 850]);
if isfield(p,'focus_time'),focus=max(0,p.focus_time-20e-6);else,focus=min(p.duration-100e-6,0.104);end
sel=d.time>focus & d.time<min(p.duration,focus+75e-6);
subplot(3,1,1);stairs(d.time(sel)*1e6,d.signals.values(sel,1));ylabel('PWM');grid on;
subplot(3,1,2);stairs(d.time(sel)*1e6,d.signals.values(sel,[3 4]));legend('Active','Shadow');grid on;
% Event rows retain sub-microsecond ADC/ISR ordering; cumulative counters
% would hide these transitions on the scale of a long-running simulation.
events=readtable(fullfile(folder,[name '_events.csv']));
kinds=[0 3 4 5 7 9 8 11 12];
labels={'ZERO','SOCA','S&H','EOC','ISR entry','Shadow write','ISR exit','Trip','Clear'};
subplot(3,1,3);hold on;
for j=1:numel(kinds)
 mask=events.kind==kinds(j) & events.time_s>=focus & events.time_s<=focus+75e-6;
 plot(events.time_s(mask)*1e6,j*ones(sum(mask),1),'.','MarkerSize',10);
end
hold off;set(gca,'YTick',1:numel(kinds),'YTickLabel',labels);ylim([0 numel(kinds)+1]);grid on;
xlabel('time (us)');saveas(f,fullfile(folder,[name '_timing.png']));close(f);
end
