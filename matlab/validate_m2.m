function validate_m2()
root=fileparts(fileparts(mfilename('fullpath')));folder=fullfile(root,'evidence','M2');
if ~isfolder(folder),mkdir(folder);end
fid=fopen(fullfile(folder,'matlab.txt'),'w');fclose(fid);diary(fullfile(folder,'matlab.txt'));
cleanup=onCleanup(@()diary('off'));disp(version);build_mex();create_event_model();
% config: TBCLK,PRD,CMPA,ADC enable,ACQ ns,convert ns,N,IRQ ns,ISR ns,Vref.
c=[10 1514 1000 1 80 250 1 120 800 3.3];T=15150*1e-9;
b=[0.2 1.1 0 1 0 400 0 2.2];changed=b;changed(1)=0.8;after=changed;after(1)=2.8;
[e,o,~]=event_case('sample_hold',[0 b;40e-9 changed;120e-9 after;100e-6 after],c,45e-6,7.3e-6,'M2');
assert_at(e,16,0,0);assert_at(e,4,80e-9,q(0.8));assert_at(e,5,330e-9,q(0.8));
assert_at(e,4,410e-9,65536+q(1.1));assert_at(e,5,660e-9,65536+q(1.1));
assert_at(e,4,740e-9,131072+q(2.2));assert_at(e,5,990e-9,131072+q(2.2));
assert_at(e,6,990e-9,0);assert_at(e,7,1110e-9,q(0.8));
assert_at(e,17,1110e-9,400);assert_at(e,9,1910e-9,400);assert_at(e,8,1910e-9,0);
assert_at(e,1,T,400);
fprintf('S&H PASS: 0.8V at 80ns retained through 330ns EOC despite 2.8V input at 120ns. ISR entry=1110ns, write/exit=1910ns.\n');
% Independent analog values in N=4 acquisition windows (not copied samples).
n=c;n(7)=4;v=b;v(1)=0.1;s=[0 v];
for j=1:4,v(1)=0.1+0.2*j;s=[s;j*300e-9 v];end %#ok<AGROW>
s=[s;100e-6 v];[en,on,~]=event_case('oversampling',s,n,30e-6,7.3e-6,'M2');
samples=en(en.kind==4 & en.data<65536,:);codes=q([0.1 0.3 0.5 0.7]);
assert(max(abs(samples.time_s(1:4)-[80;410;740;1070]*1e-9))<1e-12);
assert(isequal(samples.data(1:4),codes(:)));assert(numel(unique(samples.data(1:4)))==4);
assert_at(en,18,1320e-9,floor((sum(codes)+2)/4));
fprintf('N=4 PASS: IL codes %s, averaged=%d at 1320ns; each acquisition has distinct input.\n',mat2str(codes),floor((sum(codes)+2)/4));
% ADC clipping and reference convention: 4096 bins, nearest-code, saturate.
levels=[-0.1 0 1.65 3.3 4.2];
for j=1:numel(levels)
 v=b;v([1 2 8])=levels(j);
 [x,~,~]=event_case(sprintf('quant_%d',j),[0 v;100e-6 v],c,2e-6,7.3e-6,'M2');
 r=x.data(x.kind==18);assert(isequal(mod(r(1:3),65536),repmat(q(levels(j)),3,1)));
end
% ISR completing before, exactly at, after ZERO. Observe actual falling edge.
for finish=[15140 15150 15160]
 d=c;d(9)=finish-1110;
 [x,~,~]=event_case(sprintf('isr_%dns',finish),[0 b;100e-6 b],d,50e-6,7.3e-6,'M2');
 assert_at(x,9,finish*1e-9,400);
 if finish<15150,loadtime=T;else,loadtime=2*T;end
 assert_at(x,1,loadtime,400);assert_at(x,2,loadtime+4e-6,400);
 if finish>=15150,assert(~any(x.kind==1 & x.data==400 & abs(x.time_s-T)<1e-12));end
end
% ADC remains busy at new SOCA; request dropped explicitly, not overwritten.
d=c;d(5)=10000;d(6)=10000;
[x,~,~]=event_case('adc_overrun',[0 b;100e-6 b],d,65e-6,7.3e-6,'M2');
assert(sum(x.kind==13)==3);assert_at(x,13,T,1);assert_at(x,6,60e-6,0);
% Long ISR, one pending snapshot, next IRQ lost; Trip does not wait for CPU.
d=c;d(9)=50000;v=b;v(1)=0.4;w=b;w(1)=0.8;high=w;high(3)=3.3;
s=[0 b;15e-6 v;30e-6 w;31.333333e-6 high;40e-6 w;100e-6 w];
[x,ox,~]=event_case('isr_overrun_trip',s,d,65e-6,7.3e-6,'M2');
assert_at(x,14,2*T+990e-9,1);assert_at(x,11,31.333333e-6,1);
assert_at(x,10,31.333333e-6,0);assert_at(x,7,51110e-9,q(0.4));
assert(~any(x.kind==10 & x.data==1 & x.time_s>31.333333e-6));
assert(any(ox.trace.signals.values(:,11)==1 & ox.trace.signals.values(:,15)==1));
f=figure('Visible','off');subplot(2,1,1);
scatter(e.time_s(e.kind==4)*1e6,mod(e.data(e.kind==4),65536),'filled');hold on;
scatter(e.time_s(e.kind==5)*1e6,mod(e.data(e.kind==5),65536));legend('S&H','EOC');ylabel('ADC code');grid on;
subplot(2,1,2);stairs(o.trace.time*1e6,o.trace.signals.values(:,[3 4 11]));
legend('Active','Shadow','ISR running');xlabel('time (us)');grid on;saveas(f,fullfile(folder,'adc_isr.png'));close(f);
save(fullfile(folder,'selected_traces.mat'),'o','on','ox');
fprintf('M2 PASS: ACQ/EOC/12bit/saturation; independent N=4; IRQ latency/execution; before/at/after ZERO; ADC/IRQ overrun; pending snapshot; Trip while ISR blocked.\n');
close_system('dsp_event_test',0);
end
function code=q(v)
code=min(4095,max(0,floor(v*4096/3.3+0.5)));
end
function assert_at(e,kind,t,data)
assert(any(e.kind==kind & abs(e.time_s-t)<1e-12 & e.data==data),...
 'Missing event kind=%d t=%.12g data=%g',kind,t,data);
end
