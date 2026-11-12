function validate_m1()
root=fileparts(fileparts(mfilename('fullpath'))); folder=fullfile(root,'evidence','M1');
if ~isfolder(folder), mkdir(folder); end
fid=fopen(fullfile(folder,'matlab.txt'),'w');fclose(fid);
diary(fullfile(folder,'matlab.txt')); cleanup=onCleanup(@() diary('off'));
disp(version); disp(ver('simulink')); disp(mex.getCompilerConfigurations('C','Selected'));
build_mex(); create_event_model(); base=[0 0 0 1 0 1000 0 0];
period=15150*1e-9; config=[10 1514 1000];
[e,out,elapsed]=event_case('count',[0 base;100e-6 base],config,60e-6,7.3e-6);
assert_times(e,0,(0:3)'*period,1e-12); assert_times(e,2,(0:3)'*period+10e-6,1e-12);
assert_times(e,15,(0:2)'*period+15140e-9,1e-12); assert_times(e,3,(0:3)'*period,1e-12);
fprintf('PWM: TBPRD=1514, period=1515 ticks, f=%.9f Hz, error=%.6f%%\n',1/period,(1/period/66000-1)*100);
fprintf('count: %d committed events, %d solver times, %.6f s wall; 60us / 10ns reference: 6000 ticks\n',height(e),numel(out.tout),elapsed);
assert(numel(out.tout)<800,'Solver traverses TBCLK grid');
tr=out.trace; rises=tr.time([true;diff(tr.signals.values(:,1))>0]);
assert(max(abs(rises(1:4)-(0:3)'*period))<1e-12);
prdt=e.time_s(e.kind==15);
for j=1:numel(prdt)
 [~,ix]=min(abs(tr.time-prdt(j)));assert(tr.signals.values(ix,2)==1514);
end
f=figure('Visible','off');
subplot(2,1,1);stairs(tr.time*1e6,tr.signals.values(:,1));ylabel('PWM');grid on;
subplot(2,1,2);stairs(tr.time*1e6,tr.signals.values(:,2));ylabel('TBCTR');xlabel('time (us)');grid on;
saveas(f,fullfile(folder,'count.png'));close(f);
for timing=[15140 15150 15160]
 command=base; command(6)=400; command(7)=1;
 [x,~,~]=event_case(sprintf('shadow_%dns',timing),[0 base;timing*1e-9 command;90e-6 command],config,65e-6,7.3e-6);
 loadtime=x.time_s(x.kind==1 & x.data==400);
 if timing<15150, expected=period; else, expected=2*period; end
 assert(abs(loadtime(1)-expected)<1e-12);
 edge=x.time_s(x.kind==2 & x.data==400); assert(abs(edge(1)-(expected+4e-6))<1e-12);
end
for cmp=[0 1514]
 b=base;b(6)=cmp;
 [x,~,~]=event_case(sprintf('boundary_%d',cmp),[0 b;70e-6 b],[10 1514 cmp],45e-6,7.3e-6);
 if cmp==0, assert(~any(x.kind==10 & x.data==1)); else, assert_times(x,2,[15140;30290]*1e-9,1e-12); end
end
b=base;high=b;high(3)=3.3;clear=b;clear(5)=1;
s=[0 b;1.234567e-6 high;8e-6 b;25e-6 clear;26e-6 b;70e-6 b];
[x,tripout,~]=event_case('trip',s,config,65e-6,7.3e-6);
assert_times(x,11,1.234567e-6,1e-12); assert_times(x,12,2*period,1e-12);
off=x.time_s(x.kind==10 & x.data==0);assert(abs(off(1)-1.234567e-6)<1e-12);
assert(~any(x.kind==10 & x.data==1 & x.time_s>2e-6 & x.time_s<2*period-1e-12));
[x,zout,~]=event_case('trip_zero',[0 b;period high;70e-6 high],config,45e-6,7.3e-6);
assert(~any(x.kind==10 & x.data==1 & x.time_s>=period-1e-12));
assert(~any(zout.trace.signals.values(zout.trace.time>=period,1)));
held=b;held(5)=1;heldhigh=held;heldhigh(3)=3.3;
[x,~,~]=event_case('held_clear',[0 held;1.234567e-6 heldhigh;8e-6 held;70e-6 held],config,45e-6,7.3e-6);
assert(~any(x.kind==12 & x.time_s>1e-6));
unsafe=b;unsafe(4)=0;unsafehigh=unsafe;unsafehigh(3)=3.3;
unsafeclr=unsafe;unsafeclr(5)=1;reenable=unsafeclr;reenable(4)=1;
[x,~,~]=event_case('unsafe_clear',[0 unsafe;1e-6 unsafehigh;2e-6 unsafe;3e-6 unsafeclr;8e-6 reenable;70e-6 reenable],config,45e-6,7.3e-6);
assert(~any(x.kind==12));assert(~any(x.kind==10 & x.data==1));
assert(any(x.kind==11 & abs(x.time_s-1e-6)<1e-12));
f=figure('Visible','off');t=tripout.trace.time;y=tripout.trace.signals.values;
stairs(t*1e6,y(:,[1 5]));xlabel('time (us)');legend('PWM','Trip latch');grid on;
saveas(f,fullfile(folder,'trip.png'));close(f);
fprintf('M1 PASS: count/PRD/CMPA/SOCA; shadow before/at/after ZERO; CMPA bounds; fractional-ns trip/latch/rearm/priority.\n');
close_system('dsp_event_test',0);
end
function assert_times(e,kind,expected,tol)
actual=e.time_s(e.kind==kind);assert(numel(actual)>=numel(expected));
assert(max(abs(actual(1:numel(expected))-expected(:)))<tol);
end
