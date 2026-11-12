# AI_CCM_BOOST_PFC

已在本机 MATLAB/Simulink 运行并验收的 C99 数字外设事件模型与 AVG-CCM
Boost PFC 教学工程。使用 Issue #1 最新规格：220 Vac/50 Hz → 400 V、3300 W，
220 µH、1200 µF，66 kHz 目标频率（100 MHz TBCLK、TBPRD=1514，实际 66.0066 kHz）。

## 一键复现

需要 MATLAB、Simulink 和兼容的 C MEX 编译器；实测环境为 R2021b / Simulink 10.4 /
MinGW64 6.3.0。无需 Simscape、Simscape Electrical 或第三方功率级工具箱。
在 MATLAB 切换至仓库目录后运行：

```matlab
addpath('matlab'); run_all
```

自动编译 C MEX、生成/加载 `.slx`、运行 M1/M2/M3、执行断言、保存波形及事件证据。
首次构建会复用本机已安装的 MinGW；其他机器先执行 `mex -setup C`。
单阶段可分别运行 `validate_m1`、`validate_m2`、`validate_m3`。

## 已验证的结果

- M1：ZERO/CMPA/PRD/SOCA、Shadow 前/同时/后写入、异步 Trip/锁存/安全解除；
  60 µs 只有 25 个事件/25 个求解器时间点，短时 10 ns 参考为 6001 点。
- M2：窗口末端 S&H、12-bit 量化、N=4 独立采样平均、ISR 延迟/执行/跨 ZERO、
  ADC/IRQ 积压和丢弃、保留待处理快照、ISR 忙时的 Trip。
- M3：480 V→3.3 V 双路电压标定；0.05 V/A、500 kHz 模拟前端；
  30 ns / 1 mV / 3.3 V 比较器；真实开关功率级闭环、负载阶跃和模拟过流。

3.3 kW 稳态实测：母线平均约 400.80 V、纹波 20.93 Vpp、总 PF 0.9923；
PWM 周期平均电流的 2～40 次谐波 THD 约 5.22%。3300→2475 W 阻性负载阶跃，
末段母线约 400.87 V。测量窗口、效率、局部 CCM 条件及全部限制见验证文档。
这些是虚拟教学模型结果，不表示 F280039C 或实物保护等效。

## 文件与证据

- `matlab/ccm_boost_pfc.slx`：独立 Simulink 功率级/模拟前端 + C99 S-Function。
- `matlab/dsp_event_test.slx`、`pfc_analog_test.slx`、`pwm_tick_reference.slx`：验收工程。
- `src/dsp_behavior.c`、`src/pfc_control.c`：事件外设与独立 AVG 控制器。
- `evidence/M1`、`M2`、`M3`：MATLAB 日志、CSV、PNG、完整事件日志 `.csv.gz`。
  大型原始 MAT/CSV 留在本地，可用脚本重新生成。
- [架构/参数/事件优先级](docs/ARCHITECTURE.md)、[验证/失败案例/限制](docs/VALIDATION.md)、
  [故障链证据](docs/FAULT_CHAINS.md)、[本机资产与公开范围审计](docs/LOCAL_ASSETS.md)。

功能验收全部在 MATLAB/Simulink 完成；`make test` 只作 C 编译辅助检查。
没有使用 Python 仿真或 Python 测试代替验收，也没有复制私有工程代码/电路。
