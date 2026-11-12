% Plot event time ordering produced by `./build/trace_demo > events.csv`.
% MATLAB 2014b-compatible: char/cellstr, not string array features.
T = readtable('events.csv');
t_us = T.time_ns * 1e-3;
figure('Name', 'Digital peripheral model: event timing');
subplot(3,1,1);
stairs(t_us, T.pwm_out, 'LineWidth', 1.2);
ylim([-0.1 1.1]); ylabel('PWM'); grid on;
subplot(3,1,2);
stairs(t_us, T.cmpa_active, 'LineWidth', 1.1); hold on;
stairs(t_us, T.cmpa_shadow, '--'); hold off;
ylabel('CMPA counts'); legend('Active', 'Shadow'); grid on;
subplot(3,1,3);
labels = {'ZERO','SOCA','ACQ_END','EOC','ADC_INTERRUPT', ...
          'ISR_ENTRY','SHADOW_WRITE','SHADOW_LOAD','CMPA'};
for k = 1:length(labels)
    ix = strcmp(T.event, labels{k});
    plot(t_us(ix), k*ones(sum(ix),1), '.'); hold on;
end
hold off; set(gca, 'YTick', 1:length(labels), 'YTickLabel', labels);
xlabel('Simulated time (us)'); ylabel('Hardware / CPU events'); grid on;
