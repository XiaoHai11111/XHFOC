# XHFOC_STM32G4_FW


## 当前硬件配置

- 平台：`STM32G431`，基于 CubeMX 生成工程，采用 `HAL + FreeRTOS（CMSIS-RTOS v2）`。
- 当前状态：外设初始化、IRQ/DMA、FOC 控制、USART3/USB CDC 收发与命令解析链路均已接入。

### 时钟 / RTOS / 中断基线
- HAL 时基来源：`TIM6` 中断（`HAL_TIM_Base_Start_IT`）。
- FreeRTOS 可调用中断边界：`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`。
- 常用中断优先级：
  - DMA/ADC/FDCAN/TIM1/USB：`5`
  - USART3：`6`
  - PendSV/SysTick：`15`

### DMA 通道映射
- `DMA1_Channel1` -> `USART3_RX`（`DMA_CIRCULAR`）
- `DMA1_Channel2` -> `USART3_TX`（`DMA_NORMAL`）
- `DMA1_Channel3` -> `ADC2`（`DMA_CIRCULAR`）
- `DMA1_Channel4` -> `ADC1`（`DMA_CIRCULAR`）
- `DMA1_Channel5` -> `TIM2_CH1`（`DMA_NORMAL`，`PERIPH_TO_MEMORY`）

### 外设技术路线
- USART3：`IDLE/半满/满事件 + 128 B RX DMA(循环) + TX DMA(普通)`，串口参数 `921600 / 8N1`。
- ADC1/ADC2：`ADC中断 + DMA循环`，注入组由 `TIM1_CC4` 下降沿触发。
- TIM1：中心对齐 PWM（`CH1/CH2/CH3 + 互补输出`），`TIM1_UP_TIM16` 与 `TIM1_TRG_COM_TIM17` 中断使能。
- TIM2：`CH1` PWM 已配置，`TIM2_CH1` DMA 已预连线。
- TIM3：基础定时器已初始化，当前应用未启用额外数据链路。
- FDCAN1：`FDCAN_FRAME_FD_BRS`，中断驱动（`IT0 + IT1`）。
- SPI1：主机模式 2 线 16 位，当前未接 DMA/中断数据链路。
- USB Device（CDC FS）：在 `defaultTask` 中启动（`MX_USB_Device_Init -> USBD_Start`），使用 USB LP/HP 中断 + PCD 回调。

## 当前硬件配置快照

- 平台：`STM32G431`，基于 CubeMX 生成，使用 HAL + FreeRTOS（CMSIS-RTOS v2）。
- 当前状态：已完成外设初始化、IRQ/DMA、FOC 控制和应用层通信链路。

### 时钟/RTOS/中断基线

- HAL 时基来源：`TIM6` 中断（`HAL_TIM_Base_Start_IT`）。
- FreeRTOS 中断边界：`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`。
- 常用外设中断优先级：
  - DMA/ADC/FDCAN/TIM1/USB：优先级 `5`
  - USART3：优先级 `6`
  - 内核关键中断：`PendSV/SysTick` 优先级 `15`

### DMA 通道映射

- `DMA1_Channel1`：`USART3_RX`（`DMA_CIRCULAR`）
- `DMA1_Channel2`：`USART3_TX`（`DMA_NORMAL`）
- `DMA1_Channel3`：`ADC2`（`DMA_CIRCULAR`）
- `DMA1_Channel4`：`ADC1`（`DMA_CIRCULAR`）
- `DMA1_Channel5`：`TIM2_CH1`（`DMA_NORMAL`，`PERIPH_TO_MEMORY`）

### 外设技术路线

- USART3：
  - 异步串口，`921600`，`8N1`，TX/RX 使能；有效 RX DMA 环形缓冲为 `128 B`。
  - 路线：`IDLE/半满/满事件通知 + RX DMA(循环) + TX DMA(普通)`，UART 任务阻塞等待事件，不再每 `1 ms` 轮询。
- ADC1/ADC2：
  - 规则组已配置（扫描模式），双 ADC 独立模式。
  - 注入组触发源：`TIM1_CC4` 下降沿。
  - 路线：`ADC中断 + DMA循环`。
- TIM1：
  - 中心对齐 PWM（`CH1/CH2/CH3 + 互补输出`），用于电机控制基波形。
  - `TIM1_UP_TIM16` 与 `TIM1_TRG_COM_TIM17` 中断已使能。
- TIM2：
  - `CH1` PWM 已配置。
  - `TIM2_CH1` 的 DMA 已预接好。
- TIM3：
  - 基本定时器已初始化（内部时钟），当前应用未启用额外链路。
- FDCAN1：
  - `FDCAN_FRAME_FD_BRS`，正常模式，自动重发使能。
  - 路线：中断驱动（`IT0 + IT1`）。
- SPI1：
  - 主机模式、2 线、16 位，当前未配置 DMA/中断数据链路。
- USB Device（CDC FS）：
  - 在 `defaultTask` 中启动（`MX_USB_Device_Init` -> `USBD_Start`）。
  - 路线：USB LP/HP 中断 + PCD 端点回调。



## 移植模块技术路线实施

### 通信架构（USART3 + USB CDC）

- 总体分层：`传输层(USART3/USB)` -> `协议层(ASCII/CMD)` -> `业务层(Motor/参数读写/状态查询)`。
- 启动任务生命周期：`defaultTask` 完成 USB 与应用初始化后退出；`commTask` 构造协议树并创建 UART/USB 服务任务后退出，不再周期性空唤醒。
- USART3 技术路线：`中断 + DMA` 组合。
  - 接收：`Receive-to-IDLE + RX DMA 循环模式` 持续搬运数据，IDLE/半满/满回调只通知 UART 任务解析。
  - 发送：`TX DMA 普通模式`，按帧触发发送完成回调。
  - 同步：通过 `FreeRTOS 信号量` 与任务解耦，中断仅做事件投递。
- USB CDC 技术路线：`端点回调 + 接收队列 + 发送互斥`。
  - 接收回调写入软件缓冲并投递 `sem_usb_rx`。
  - 协议任务在任务上下文解析命令，避免在中断执行重逻辑。
  - 发送侧通过 `sem_usb_tx` 保证单次发送窗口，防止重入覆盖。
  - 端点规划：`EP1(CDC VCP)` + `EP3(Native/ODrive)` 双端点并行通信。
  - Native/Fibre 对象树通过 JSON 描述符发布 `motor` 方法；电流、速度和位置分别使用
    `set_current`、`set_velocity` 和 `set_position`。

### FOC 与 RTOS 实时诊断

- FreeRTOS 调度器启动时使能 Cortex-M4 DWT `CYCCNT`：
  - 从 `ADC1_2_IRQHandler` 入口到 `focControlTask` 从通知等待中恢复的最大延迟记录为
    `wake_max`，包含 HAL ADC 中断处理、采样快照以及 ISR 到任务切换时间。
  - `focMotor.Tick()` 的最大执行周期数记录为 `tick_max`；两项同时按当前
    `SystemCoreClock` 换算为微秒。
- ADC 通知使用计数型任务通知。若一次任务唤醒时累计了多个 ADC 事件，本轮只执行一次
  `focMotor.Tick()`，其余事件累计到 `missed`，用来直接识别控制任务未跟上 10 kHz 触发的问题。
- 每 `10 s` 由现有 `peripheralTask` 输出一次诊断，不新增周期任务：
  - `[rtos]`：累计 FOC tick、missed-tick、最大执行时间、最大唤醒延迟和 FreeRTOS 堆余量。
  - `[rtos.task]`：上一统计窗口内各活动任务的 CPU 占比、优先级、状态和
    `stack_min_free`（任务创建以来的最小剩余栈，单位 B）。
- 任务运行时间统计使用 DWT 驱动的 FreeRTOS run-time stats；软件计数器按 `256` 个 CPU
  周期为一单位，降低上下文切换测量开销。任务快照最多容纳 `10` 个活动任务，快照缓冲在
  报告期间从 FreeRTOS 堆临时申请并立即释放；超出容量或堆不足会输出明确的 `[rtos]` 错误。

### USB Native/Fibre JSON 对象接口

三个接口在 `CmdCtrlMotor::MakeProtocolDefinitions()` 中与 `start`、`stop` 一起发布。连接
`4.Software/CLI-Tool` 后，在交互 Shell 中调用：

```python
foc0.motor.set_current(1.0)
foc0.motor.set_velocity(5.0)
foc0.motor.set_position(12.5663706)
```

- `set_current(target)`：q 轴目标电流，单位 A。
- `set_velocity(target)`：目标机械角速度，单位 rad/s。
- `set_position(target)`：上电后累计坐标系中的绝对多圈机械角，单位 rad；例如 `4π` 为正向两圈，不会折算到单圈最短路径。
- 三个方法会设置对应闭环模式和目标，并返回是否接受。同一模式下重复更新目标时保持运行与轨迹连续；只有实际切换模式时才停机、原子更新模式与目标、复位控制器状态并恢复原运行状态。
- 这些调用走 USB Native/ODrive 端点及自动生成的 JSON 接口描述，不是 CDC 文本 JSON；原有 CDC `!START`、`!STOP` 命令继续有效。

### 多圈位置轨迹

- 位置反馈与误差均使用累计机械角，不进行单圈最短路径折返。
- FOC 初始化完成后，位置模式默认目标设置为当时的实际多圈位置；仅发送 `!START` 会保持当前位置，不再隐式跳到硬编码 `3.14 rad`。
- 位置环使用 `P=5`，输出限制为 `5 rad/s`，并使用 `20 rad/s²` 加速度限制。原 `P=10` 在突发扰动后表现为低频欠阻尼，已回退。
- 大位置阶跃根据剩余距离和制动距离生成加速、巡航、减速速度目标，避免到达目标附近才突然反向。
- 位置模式单独使用 `2 A` 恢复电流上限；外力突然推动电机时，不再直接使用全局 `10 A` 上限进行猛烈反向制动。电流指令继续使用原来已验证的全局斜率，避免额外执行器延迟。
- 速度反馈低通时间常数保持已验证的 `60 ms`；`20 ms` 在当前编码器速度量化条件下会引发无扰动自激，禁止继续使用。
- PID 在输出饱和且积分继续推向饱和方向时停止积分，减少长距离运行后的积分残留与往复振荡。
- PID 和低通滤波器使用完整 64 位微秒时间戳，避免长时间运行后的周期计算量化和 32 位截断。

### 灯语系统（LED 状态机）

- 设计目标：统一表达 `STOP/RUNNING/FINISH/NO_CALIB/STALL/OVERLOAD`。
- 实现路径：
  - `Ctrl/Signal/LED` 负责灯语序列与状态映射。
  - `Platform/Signal/LED` 负责 GPIO 落地驱动。
  - 周期任务轮询状态并驱动三色灯输出。
- 时序约束：每种模式约 `800ms` 完成一次完整序列，便于联调和故障定位。

### MT6816 编码器（SPI 采样链路）

- 分层结构：
  - `Ctrl/Sensor/Encoder/mt6816_base.*`：角度语义、校验与无磁判断。
  - `Platform/Sensor/Encoder/mt6816_stm32.*`：SPI 收发与片选时序。
- 数据路径：`SPI1 收发` -> `raw` -> `rect` -> `校验位/无磁标志` -> 控制环使用。
- 调试路径：通过 USB CDC 周期输出 `raw/rect/chk/nomag` 关键字段。

### EEPROM（模拟 Flash 持久化）

- 方式：链接脚本保留 Flash 最后一个 2 KiB 擦除页（`0x0801F800`–`0x0801FFFF`），当前使用其中前 256 B 作为逻辑配置区并提供字节级读写。
- 结构：
  - `Platform/Memory/random_flash_utils.*`：页擦写与缓冲刷写。
  - `Platform/Memory/random_flash_interface.*`：EEPROM 类接口封装。
  - `Platform/Memory/encoder_calibration_storage.*`：带 magic、schema、长度和 CRC32 的 FOC 编码器对齐记录。
- 路线：
  - 先读入 RAM 缓冲，再按需写回 Flash，降低擦写频次。
  - 首次启动或记录无效时执行 FOC 编码器方向/电角度零偏对齐并保存；后续启动验证记录后直接复用。
  - MT6816 旧的 32 KiB 非线性查表地址来自 STM32F103，当前 G431 固件未为其保留空间，因此显式禁用，避免与应用镜像和配置页重叠。

### ADC采样移植技术路线（补充）

####  目标与分层

- 目标：完成 9 路模拟量统一采样与上层接口统一读取，并用于通信输出与后续控制闭环。
- 分层结构：
  - `Core/Src/adc.c`：底层 ADC1/ADC2 配置、DMA 循环搬运、统一 raw/voltage 接口。
  - `Ctrl/Sensor/*Sense`：传感器算法抽象层（电流/电压/ADSPE/NTC）。
  - `Platform/Sensor/*Sense`：STM32 平台适配层（通道映射、读取与换算）。
  - `UserApp/main.cpp`：任务中调用采样接口并通过 USB CDC 打印。

####  通道映射与采样链路

- ADC1（规则组 + DMA循环）：
  - Rank1: IA (`ADC1_IN1`)
  - Rank2: IB (`ADC1_IN2`)
  - Rank3: IC (`ADC1_IN3`)
  - Rank4: NTC (`ADC1_IN11`)
- ADC2（规则组 + DMA循环）：
  - Rank1: VA (`ADC2_IN6`)
  - Rank2: VB (`ADC2_IN7`)
  - Rank3: VC (`ADC2_IN8`)
  - Rank4: ADSPE (`ADC2_IN5`)
  - Rank5: VBUS (`ADC2_IN11`)
- 运行链路：
  - 系统启动后在 `Main()` 调用 `AdcStartDmaSampling()`
  - ADC 连续转换，DMA 循环更新缓冲
  - 上层通过 `AdcGetRaw()` / `AdcGetVoltage()` 取值

#### 计算公式（IA 等）

##### ADC原始码值转电压

- `Vadc = Raw * 3.3 / 4095`
- 其中：
  - `Raw`：12bit ADC 原始值（0~4095）
  - 3.3V：ADC参考电压

##### 三相电流 IA/IB/IC

- 硬件模型（运放差分）：
  - `uo = k * (u2 - u1)`
  - `k = Rf / R`
  - `u2 - u1 = Iphase * Rm`
- 电流换算：
  - `Iphase = (Vadc - Voffset) / (Rm * k)`
- 当前代码实现等价形式：
  - `Iphase = (Vadc - zeroOffset) * gain`
  - `gain = 1 / (Rm * AmpGain)`
- 当前参数（已接入）：
  - `Rm = 1mΩ = 0.001Ω`
  - `AmpGain(k) = 20`
  - `zeroOffset` 由上电静态校准得到（接近 1.65V）

##### 相电压/母线电压 VA/VB/VC/VBUS

- 基础：
  - `Vmeas = Raw * 3.3 / 4095`
- 若有分压网络，使用分压系数还原：
  - `Vreal = Vmeas * Kdiv`
- 当前代码默认 `Kdiv = 1`，后续按硬件实测阻值修正。

#####  ADSPE

- `Vadspe = Raw_adspe * 3.3 / 4095`
- 若前端有比例关系，同样按 `Vreal = Vadspe * Kadspe` 修正。

##### NTC温度

- 先由 ADC 得到 NTC 电压：
  - `Vntc = Raw_ntc * 3.3 / 4095`
- 上拉分压求 NTC 电阻：
  - `Rntc = Rpullup * Vntc / (Vref - Vntc)`
- Beta 模型求温度：
  - `T(K) = 1 / ( 1/T0 + ln(Rntc/R0)/B )`
  - `T(°C) = T(K) - 273.15`
- 当前默认参数：
  - `Rpullup = 10kΩ`
  - `R0 = 10kΩ @ 25°C`
  - `B = 3950`

#### 校准建议

- 电流零点校准：电机不通电、PWM关闭状态下执行 `zeroOffset` 校准。
- 电压系数校准：用万用表实测 VA/VB/VC/VBUS，对齐 `Kdiv`。
- 温度校准：按实际 NTC 型号（`R0/B`）替换默认值。

## git提交备注

```text
feat(comm): 完成通信架构与功能移植，接入USB双端点并修复Native接口卡死(通过注释灯语任务解决，后面再放开注释就没有卡死现象，可能是任务存在内存泄漏，后续遇到需要进行彻底解决)

[背景]
- 问题/需求: FOC通过精确控制电机的磁场方向，实现高效、稳定运行；项目需要统一通信架构、异步收发、命令解析与外设功能闭环。
- 触发条件: 现工程已完成CubeMX外设初始化，但通信链路与业务模块需从 RTOS_MT6816_FW 完整移植并联调；在 cli-tool 连接 `XH 1.0 Native Interface` 时出现通信卡死。

[改动]
- 新增目录:
  - 3rdParty/fibre
  - Ctrl/Motor
  - Ctrl/Sensor/Encoder
  - Ctrl/Signal/LED
  - Platform/CmdCtrlMotor
  - Platform/Communication
  - Platform/Memory
  - Platform/Sensor/Encoder
  - Platform/Signal/LED
  - UserApp
- 修改文件:
  - CMakeLists.txt
  - Core/Src/app_freertos.c
  - Core/Src/main.c
  - Core/Src/dma.c
  - Core/Src/usart.c
  - Core/Src/stm32g4xx_it.c
  - USB_Device/App/usbd_cdc_if.c
  - USB_Device/App/usbd_cdc_if.h
  - USB_Device/App/usbd_desc.c
  - USB_Device/App/usbd_desc.h
  - USB_Device/App/usb_device.c
  - USB_Device/App/usb_device.h
  - USB_Device/Target/usbd_conf.c
  - USB_Device/Target/usbd_conf.h
  - Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc/usbd_cdc.h
  - Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c
  - README.md
- 关键点:
  - 完成通信架构、灯语系统、MT6816、EEPROM模块移植并接入运行链路。
  - 完成 USART3 `中断 + RX DMA(循环) + TX DMA(普通)` 异步收发链路。
  - 完成 USB 双端点通信：`CDC(EP1)` + `Native/ODrive(EP3)`，名称与源工程保持一致。
  - 修复 Native 通信卡死：拆分USB发送同步路径，避免不同端点互相阻塞导致死锁。
  - 完成命令分发/解析/执行链路接入（ASCII + Native协议路径）。

[影响评估]
- 硬件影响: 无新增硬件连接，复用现有 SPI1/USART3/USB/LED 引脚。
- 实时性影响: 通信与灯语任务为异步任务，主控制链路影响可控；卡死修复后通信阻塞风险显著下降。
- 资源影响: Flash/RAM 占用增加（新增通信与功能模块）；构建通过且资源占用在可用范围内。
- 兼容性影响: 保留 CubeMX 框架与工程结构，兼容现有生成流程。

[验证]
- 构建: cmake --preset Debug && cmake --build --preset Debug -j 8
- 板测:
  - [x] USB枚举为复合设备，显示 `XH 1.0 CDC Interface` 与 `XH 1.0 Native Interface`
  - [x] cli-tool 通过 `XH 1.0 Native Interface` 可持续收发，无卡死
  - [x] USART3 收发链路可正常收包/发包
  - [x] 灯语状态机轮播与 MT6816 数据输出正常

[回滚]
- git revert <this_commit_hash>
- 重点回滚文件:
  - Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c
  - USB_Device/App/usbd_cdc_if.c
  - Platform/Communication/interface_usb.cpp
  - UserApp/main.cpp

参考上面的模板帮我重新git提交备注，根据此次文档的主要修改内容
```

```
refactor(foc,current-sense): 重写电流采样链路与Clarke变换，提升电流环稳定性

[背景]
- 问题/需求: 现有电流采样与电流解算链路存在抖动/卡动风险，低边采样在部分工况下重构误差较大。
- 触发条件: 联调过程中出现电流波形不稳与异响，需要重构采样与αβ变换实现。

[改动]
- 修改内容:
  - 重写电流采样流程，统一采样数据入口与相电流获取逻辑。
  - 重写 Clarke 变换实现，规范 abc->αβ 计算路径，减少零漂/偏置对电流环影响。
  - 调整采样到控制的调用时序：在电流采样完成后触发 FOC 计算，避免采样与控制脱节。
  - 清理/移除旧的低边单相丢失重构分支，降低分支复杂度与误判概率。
- 修改文件:
  - Ctrl/Sensor/CurrentSense/current_sense_base.cpp
  - Ctrl/Sensor/CurrentSense/current_sense_base.h
  - Platform/Sensor/CurrentSense/current_sense.cpp
  - Ctrl/Motor/motor.cpp
  - （如涉及触发链路）Core/Src/adc.c、Core/Src/tim.c、UserApp/main.cpp
- 关键点:
  - 采样结果统一用于 FOC 电流解算入口（dq 计算前的数据一致性更高）。
  - Clarke 变换与相电流重构逻辑分层明确，便于后续维护与参数标定。
  - 控制主循环保持实时性，未引入额外阻塞路径。

[影响评估]
- 硬件影响: 无新增硬件依赖，继续使用现有电流采样与编码器链路。
- 实时性影响: 采样-控制链路更紧凑，减少相位延迟；电流环响应更一致。
- 资源影响: 代码结构调整为主，体积变化有限。
- 兼容性影响: 不改变外部通信协议；主要影响内部电流解算与控制行为。

[验证]
- 构建:
  - [x] `cmake --preset Release`
  - [x] `cmake --build --preset Release -j 8`
- 板测:
  - [x] 电机启动与低速运行无明显卡死
  - [x] 电流环运行稳定，D/Q 电流波动收敛改善
  - [x] 采样触发与控制调用时序符合预期（采样完成后执行控制）

[回滚]
- `git revert <this_commit_hash>`
- 重点回滚文件:
  - Ctrl/Sensor/CurrentSense/current_sense_base.cpp
  - Platform/Sensor/CurrentSense/current_sense.cpp
  - Ctrl/Motor/motor.cpp
```



