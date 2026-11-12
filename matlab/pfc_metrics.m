function metrics=pfc_metrics(out,p,start,stop)
tr=out.physical_trace;t=tr.time;y=tr.signals.values;
% Time-weighted measurements; do not average variable-step samples equally.
selected=t>=start & t<=stop;tt=t(selected);yy=y(selected,:);
iac=sign(yy(:,1)).*yy(:,3);duration=tt(end)-tt(1);
vmean=trapz(tt,yy(:,4))/duration;
irms=sqrt(trapz(tt,iac.^2)/duration);vrms=sqrt(trapz(tt,yy(:,1).^2)/duration);
pin=trapz(tt,yy(:,1).*iac)/duration;
pout=trapz(tt,yy(:,5).*yy(:,4).^2)/duration;
% Integrate actual switching current per PWM cycle, then fit harmonics 1..40.
z=(ceil(start/p.period):floor(stop/p.period))'*p.period;
integral_i=cumtrapz(t,y(:,3));cycle_i=diff(interp1(t,integral_i,z))./p.period;
mid=(z(1:end-1)+z(2:end))/2;iac_cycle=cycle_i.*sign(sin(2*pi*p.line_hz*mid));
X=ones(numel(mid),81);
for h=1:40,X(:,2*h)=sin(h*2*pi*p.line_hz*mid);X(:,2*h+1)=cos(h*2*pi*p.line_hz*mid);end
coeff=X\iac_cycle;amplitude=hypot(coeff(2:2:end),coeff(3:2:end));
THD=sqrt(sum(amplitude(2:end).^2))/amplitude(1);
% Switching-cycle extrema and CCM range (exclude exact line zero only).
edges=(ceil(start/p.period):floor(stop/p.period))'*p.period;
bin=discretize(tt,edges);valid=~isnan(bin);
mins=accumarray(bin(valid),yy(valid,3),[numel(edges)-1 1],@min,NaN);
maxs=accumarray(bin(valid),yy(valid,3),[numel(edges)-1 1],@max,NaN);
phase=abs(sin(2*pi*p.line_hz*(edges(1:end-1)+p.period/2)));
ccm=mean(mins(phase>0.05)>0.01);
dig=out.digital_trace.signals.values;
metrics=struct('vbus_mean_V',vmean,'vbus_pp_V',max(yy(:,4))-min(yy(:,4)),...
 'input_rms_A',irms,'input_power_W',pin,'output_power_W',pout,'PF_total',pin/(vrms*irms),...
 'THD_2_to_40_cycle_average',THD,'peak_IL_A',max(yy(:,3)),...
 'max_cycle_ripple_App',max(maxs-mins),'CCM_fraction_excluding_line_zero',ccm,...
 'events',dig(end,7),'solver_times',numel(out.tout),'mean_solver_step_s',mean(diff(out.tout)),...
 'max_solver_step_s',max(diff(out.tout)),'ADC_overrun',dig(end,13),'ISR_overrun',dig(end,14));
disp(metrics);
end
