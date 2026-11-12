function data=step_stimulus(samples)
% Explicit left/right values at every discontinuity. From Workspace's ZC
% contract recognizes duplicate timestamps with interpolation enabled.
% This advertises short pulses instead of hoping a solver trial sees them.
data=zeros(2*size(samples,1)-1,size(samples,2));data(1,:)=samples(1,:);
for j=2:size(samples,1)
 data(2*j-2,:)=[samples(j,1) samples(j-1,2:end)];
 data(2*j-1,:)=samples(j,:);
end
end
