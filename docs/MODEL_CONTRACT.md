# v0.1 DSP 外设行为模型契约与验收标准

## 1. 模拟边界

- **功率级**：由外部物理求解器（例如 Simulink）负责电感电流、母线电压、功率器件状态；`dsp_behavior.c` 不实现 Boost 功率电路。
- **数字控制算法**：`dsp_controller_fn` 读取 ISR 入栈时刻锁定的两通道 ADC 结果，返回拟写入 CMPA Shadow 的计数值；本仓库当前演示函数只返回固定 CMPA，不是 AVG PFC 控制器。
- **外设行为**：模型负责时间基准、SOC、ADC、EOC、ISR 时序、CMPA Shadow、PWM 门极逻辑和异步过流 Trip；不模拟真实 MCU 的寄存器/总线/流水线内部细节。

## 2. 时间基准与周期

所有外设时间统一使用 `uint64_t nanoseconds`（整数 ns）；TBCLK 频率由 `tbclk_ns` 指定。v0.1 只使用 **up-count**，AQ 为 **ZERO set, CMPA clear**。周期：

`T_pwm_ns = (TBPRD + 1) * TBCLK_ns`

`CMPA=0` 表示该周期 0% 高电平；CMPA=TBPRD 对应约 `TBPRD/(TBPRD+1)` 高电平，而非数学上的严格 100%。该离散约束显式保留，不偷换为连续占空比。

选择示例值：`TBCLK=10 ns`、`TBPRD=1999`、`CMPA_initial=1000`，仅为易复现配置。

## 3. 因果链（外设事件，非 CPU 轮询）

`TBCTR=ZERO` → 若有 pending，则 CMPA Shadow→Active → AQ 输出置位 → `SOCA` → 通道 IL 采样窗口打开 → acquisition 结束时锁存电压并量化 12bit → conversion 延迟结束 `EOC(IL)` → (按 oversampling 重复) → VBUS 采样转换 → 最终 `EOC(VBUS)` → `ADC_INTERRUPT` → 中断进入延迟 → ISR_ENTRY 读取 ADC 结果 → 调用独立控制代码，取得下一 CMPA 命令 → 模拟 ISR 执行时间 → ISR_EXIT 写入 Shadow → 下一次 `ZERO` 装载 Active → 新 CMPA 决定比较事件/输出。

同一时刻事件的优先级以 *硬件 ZERO/装载* 高于 ISR Shadow 写入为准，因此临界时刻 ISR 写入不能被本次 ZERO 装载；应在更严格的 MCU 周期精度模型中再次验证此时序规则。

### 当前示例（单位 ns）

| 事件 | 时间 | 说明 |
|---|---:|---|
| ZERO、SOCA | 0 | PWM 第一个周期开始 |
| IL 第 1 次采样锁存 | 80 | 结束 80 ns acquisition |
| IL 第 1 次 EOC | 330 | 再经历 250 ns conversion |
| IL 第 4 次 EOC | 1320 | 四次 12-bit 转换后平均 |
| VBUS 第 1 次 EOC | 1650 | 第二 ADC 通道转换完毕 |
| ISR_ENTRY | 1770 | 120 ns interrupt latency |
| Shadow 写入 | 2570 | 800 ns ISR execution |
| CMPA_ACTIVE 载入 | 20000 | 周期 ZERO 后才能使用新占空比 |

ADC 中每个通道使用独立的 `acq_ns` 与 `conversion_ns`，过采样数 1..16；按每次量化的数字码求平均，并采用四舍五入式整数平均。该机制是**显式顺序采样仿真约定**，不是宣称 F280039C 原生具备任意自动硬件过采样。

## 4. 独立过流保护链

`外部比较器模拟输入越过 high threshold` → `comparator_high` → **立即**锁存 `trip_latched` → `pwm_out=0`。无需 ADC、EOC、ISR、CPU 或 Shadow 更新。比较器低于 `high - hysteresis` 后，软件显式 `dsp_request_trip_clear` → **下一 ZERO** 才清除 Trip。比较器依然为高时拒绝清除。

**容易漏掉的关键条件**：该实现的“立即”仅意味着 **在调用方报告阈值交叉时间时，不额外添加 ISR/TBCLK 延迟**。Simulink 若仅在常规步长上发送模拟输入而未启用过零检测，阈值越界可能被延迟发现甚至漏掉。这属于外部求解器的接口验证责任。

## 5. 与 MATLAB/Simulink 的连接契约

S-Function / MEX Adapter 至少需要：

1. 将 Simulink 仿真时间换算成整数 ns，并通过 `dsp_next_event_ns()` 调度下一硬件事件（避免软件轮询模拟 PWM 每个 TBCLK）。
2. 在 acquisition window **结束时刻**，提供该时刻的 IL / VBUS 对应 ADC 引脚电压；这些值应已经经过电阻分压、电流检测比例等真实传感器模型。
3. 将 `dsp_pwm_output()` 映射到功率级开关输入；PWM 边沿需触发求解器执行，而不只是示波器采样点变化。
4. **异步比较器必须独立接收过零检测事件**（电流检测电压与 OCP 阈值的交叉时刻），调用 `dsp_set_comparator_voltage()`；不能依赖 ADC EOC 流程处理保护。
5. 保证外部输入事件与内部硬件事件同一时间戳时的确定性排序；回归测试需要覆盖 ZERO 与 ISR 结束的并发。
6. 控制代码应与硬件模型分离，按 ISR 入口时刻使用 ADC 寄存器快照，不能让 Simulink 端的控制器隐式读取“未来值”。
7. 必须报告时间量化、零交叉容差、计算时间、采样窗口和过采样配置；模型无法自动推断真实 C2000 的硬件延迟。

> **状态：** 以上仅是接入契约。尚未编译 MATLAB S-Function，也未运行开关功率级闭环。不能以原生 C 测试 PASS 代替 Simulink 集成测试。

## 6. 测试与反证

| 判据 | 本地测试 | 推翻模型符合需求的条件 |
|---|---|---|
| ZERO → SOCA → ADC sample/EOC → ISR → Shadow | `test_zero_soca_sampling_isr_shadow` | 事件顺序或时间戳不符合配置 |
| ISR 恰在 ZERO 时完成不会加载该次 Shadow | `test_late_isr_misses_zero` | 同时刻出现 Shadow Load 新值 |
| CMPSS 异步事件直接强制 PWM 低 | `test_asynchronous_trip_and_rearm` | OCP 事件后 PWM 继续为高 |
| OCP 高时无法清除锁存 | 同上 | 软件清除绕过独立比较器状态 |
| ADC saturation / 12bit 量化 | `test_adc_input_clipping_and_overrun` | 码超出 0..4095 或极性错误 |
| ADC SOC 背压不被静默吞没 | 同上 | 转换未完成却生成新结果 |
| ISR 堵塞时保护仍有效 | `test_isr_overrun_and_no_cpu_trip_dependency` | CPU 工作导致 OCP 无法关断 |

## 7. 尚未实现 / 未验证（明确排除）

- PWM up-down 模式、多个独立 ePWM、互补输出与 DBRED/DBFED、TBCLK phase sync、多级 SOC Event Prescaler。
- 多 ADC 模块并行转换、SOC 仲裁优先级、ADC 早期中断、实际流水线/PIE ACK 时序；这里使用单一顺序转换链和单级 pending IRQ。
- 原生 F280039C `CMPSS` 数字滤波/blanking、Trip Zone CBC、比较器传播延迟；这里只实现理想化异步 **OST** 与滞回。
- 采样保持电容充电误差、ADC 静态非线性（INL/DNL）、温漂、模拟噪声及 ETPS 精细计数。
- MATLAB S-Function / Simulink variable-step solver 集成、PFC 平均电流控制、功率级模型与端到端验证。

**下一验收阶段**应当首先证明 Simulink 在外设事件和功率级开关边沿上同步；然后以 F280039C TRM 和示波器实测修正模型参数，之后再接 AVG-CCM Boost PFC C99 控制器。
