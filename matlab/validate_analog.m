function validate_analog()
root=fileparts(fileparts(mfilename('fullpath')));folder=fullfile(root,'evidence','M3');
create_analog_model();p=pfc_parameters();p.duration=40e-6;p.max_step=2e-6;
volts=[0 220*sqrt(2) 400 480];amps=[0 1 15 sqrt(2)*15];
rows=zeros(8,7);k=0;
for j=1:4
 v=volts(j);[o,~,~]=pfc_case(sprintf('voltage_%d',j),p,'pfc_analog_test',[0 15 v v;100e-6 15 v v],'off');
 a=o.analog_trace.signals.values;d=o.digital_trace.signals.values;
 pin=v*p.voltage_gain;expected=min(4095,floor(v/480*4096+0.5));
 assert(max(abs(a(end,[2 3])-pin))<1e-9);
 assert(all(d(end,[9 10])==expected));
 k=k+1;rows(k,:)=[1 v pin a(end,2) d(end,9) expected expected*480/4096];
end
for j=1:4
 aphys=amps(j);[o,~,~]=pfc_case(sprintf('current_%d',j),p,'pfc_analog_test',[0 aphys 400 300;100e-6 aphys 400 300],'off');
 a=o.analog_trace.signals.values;d=o.digital_trace.signals.values;
 pin=aphys*p.current_sense_v_per_a*p.current_gain;expected=floor(pin/3.3*4096+0.5);
 assert(abs(a(end,1)-pin)<1e-7);assert(d(end,8)==expected);
 k=k+1;rows(k,:)=[2 aphys pin a(end,1) d(end,8) expected expected*3.3/4096/0.05];
end
calibration=array2table(rows,'VariableNames',{'channel_kind','physical_input','expected_pin_V','measured_pin_V','ADC_code','expected_code','decoded_physical'});
writetable(calibration,fullfile(folder,'calibration.csv'));disp(calibration);
% 500kHz response is simulated, not a paper conversion.
p.duration=5e-6;p.max_step=20e-9;
[o,~,~]=pfc_case('bandwidth',p,'pfc_analog_test',[0 10 400 300;10e-6 10 400 300],'off');
t=o.analog_trace.time;y=o.analog_trace.signals.values(:,1);
tau=interp1(y,t,0.5*(1-exp(-1)),'linear');measured_fc=1/(2*pi*tau);
assert(abs(measured_fc/p.current_bandwidth_hz-1)<0.002);
assert(max(abs(y-0.5*(1-exp(-t/p.current_tau))))<1e-5);
fprintf('500kHz amplifier PASS: measured tau %.6fns, fc %.6fHz; nominal %.6fns.\n',tau*1e9,measured_fc,p.current_tau*1e9);
% Ramp through actual amplifier + 1mV comparator input error + delay.
% No digital force: OC is generated solely by the analog subsystem.
p.duration=2e-6;p.max_step=2e-9;
[o,~,file]=pfc_case('analog_trip',p,'pfc_analog_test',[0 0 400 300;3e-6 180 400 300],'on');
a=o.analog_trace.signals.values;t=o.analog_trace.time;d=o.digital_trace.signals.values;td=o.digital_trace.time;
raw=find(a(:,5)>0.5,1);delayed=find(a(:,4)>1.65,1);trip_t=t(delayed);raw_t=t(raw);
propagation=trip_t-raw_t;
assert(abs(propagation-p.comparator_delay_s)<0.1e-9);
assert(abs(a(raw,1)+p.comparator_error_v-p.comparator_threshold_v)<1e-7);
e=readtable(file);tlog=e.time_s(e.kind==11);assert(abs(tlog(1)-trip_t)<1e-12);
off=e.time_s(e.kind==10 & e.data==0);assert(abs(off(1)-trip_t)<1e-12);
assert(all(d(td>=trip_t,1)==0));
fprintf('Analog comparator PASS: raw crossing %.9fns, OC HIGH %.9fns, propagation %.9fns; Trip and PWM low at OC event (<=1ps).\n',raw_t*1e9,trip_t*1e9,propagation*1e9);
f=figure('Visible','off');subplot(3,1,1);plot(t*1e6,a(:,1));ylabel('sense V');grid on;
subplot(3,1,2);stairs(t*1e6,a(:,[4 5]));legend('OC 3.3V','raw compare');grid on;
subplot(3,1,3);stairs(td*1e6,d(:,[1 5]));legend('PWM','Trip');xlabel('time (us)');grid on;
saveas(f,fullfile(folder,'analog_trip.png'));close(f);
close_system('pfc_analog_test',0);
end
