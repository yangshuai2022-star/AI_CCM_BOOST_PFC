# AI_CCM_BOOST_PFC

**AI 辅助数字电源仿真与 C99 外设行为建模**：线上技术讲座的可复现实验工程。

> 当前阶段：`v0.1` **数字外设行为模型与原生 C 回归测试**。尚未加入 CCM Boost PFC 功率级、AVG 双闭环控制器，也未在 MATLAB/Simulink 或真实 F280039C 上验证。不要将本模型直接用作硬件保护或投产代码。

## 开发顺序

1. **先定义 DSP/数字控制外设的执行行为**：PWM 计数 / CMPA / ZERO / SOCA / ADC acquisition & conversion / EOC / ISR / CMPA Shadow Load / 异步比较器 Trip。
2. 将外设模型通过可重复调度的事件时间戳与 MATLAB/Simulink 的功率级对接。
3. 将独立 C99 平均电流控制（AVG，CCM Boost PFC）连接到 ISR 计算入口。
4. 运行稳态、负载阶跃、采样相位、Shadow Load 延迟、过流保护与异常工况测试。
5. 分别报告模型内验证结果和真实 MCU 行为一致性验证结果。

## 三块架构

```text
       Simulink power stage (rectifier + Boost + DC bus)
                     ^                       |
                 PWM gate            measured analog signals
                     |                       v
       C99 peripheral behavior: ePWM / ADC / CMPSS / ISR
                     ^                       |
              shadow CMPA                  EOC IRQ
                     |                       v
          Independent C99 AVG current/voltage controller
```

## 已实现的行为（`src/dsp_behavior.c`）

- 100 MHz TBCLK 示例，50 kHz ePWM up-count（`TBPRD=1999`）；在 `ZERO` 设置 AQ，高电平在 `CMPA` 事件清除。
- `ZERO` 事件发出 `SOCA`；带通道独立 acquisition 窗口与 conversion 延迟的 ADC 顺序转换；每个采样窗口**末端**锁存 ADC 引脚电压。
- 12-bit 无符号量化与满量程钳位；逐次 SOC 过采样，平均的是每次转换的量化结果，而非理想模拟平均。
- 最后一个 EOC 置位 ADC 中断；ISR 等待进入延迟，读取 ADC 寄存器快照、计算，模拟 ISR 执行时间，退出时写 CMPA Shadow。
- 只有**下一次 `ZERO`**才能将 Shadow 载入 Active CMPA；若 ISR 写入与 `ZERO` 同时发生，则错过该次装载。
- 独立模拟比较器 OCP 可在非 TBCLK 对齐时刻立即强制 PWM 关闭；Trip 保持锁存，比较器释放且软件申请后在 `ZERO` 恢复。
- ADC SOC 积压、ISR 中断积压会记录计数与事件，防止静默掩盖处理能力不足。

注意：本版本将比较器传播延迟理想化为 0；真正的异步阈值越界时刻必须由**外部求解器过零检测**或调用方显式通知，不能靠 50 kHz 采样轮询去假装实现异步保护。

## 本地回归测试

需要支持 C99 的 gcc/clang 和 make：

```sh
make test
make build/trace_demo
./build/trace_demo > events.csv
```

`matlab/plot_events.m` 可在 MATLAB 中绘制 CSV 的事件时序。CSV 示例与回归测试**不依赖** Simulink，也不声称已经完成闭环功率级仿真。

## 工程结构

```text
include/dsp_behavior.h       外设模型接口与时间基准契约
src/dsp_behavior.c           C99 离散事件调度内核
tests/test_dsp_behavior.c   关键时序、安全关断、队列拥塞回归测试
examples/trace_demo.c        事件 CSV 生成示例
matlab/plot_events.m         事件时序展示
docs/MODEL_CONTRACT.md       事件顺序、配置假设、Simulink 集成要求
docs/LECTURE_PLAN.md        90 分钟讲座示例路线
```

## 参数来源和芯片一致性

本项目示例中的 `TBCLK`、ADC 窗口、转换、ISR 耗时、比较器阈值为**教学假设**，不是从 F280039C TRM/器件数据手册提取的最小/最大硬件指标。时间参数应由最终器件的数据手册、TRM、编译器生成汇编及实机波形分别校核。

本版本刻意不引入 DSP DriverLib、寄存器映射、动态内存、RTOS 或复杂的调度框架。下一阶段先接 Simulink 的求解器时间与事件输入，再接 PFC 控制算法，不急于实现所有 MCU 外设特性。
