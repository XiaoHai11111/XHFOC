# XHFOC_STM32G4_FW

## 当前硬件配置

- 平台：`STM32G431`，基于 CubeMX 生成工程，采用 `HAL + FreeRTOS（CMSIS-RTOS v2）`。
- 当前状态：外设初始化与 IRQ/DMA 连线已完成，应用层收发与命令解析链路尚未接入。

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
- USART3：`UART中断 + RX DMA(循环) + TX DMA(普通)`，串口参数 `115200 / 8N1`。
- ADC1/ADC2：`ADC中断 + DMA循环`，注入组由 `TIM1_CC4` 下降沿触发。
- TIM1：中心对齐 PWM（`CH1/CH2/CH3 + 互补输出`），`TIM1_UP_TIM16` 与 `TIM1_TRG_COM_TIM17` 中断使能。
- TIM2：`CH1` PWM 已配置，`TIM2_CH1` DMA 已预连线。
- TIM3：基础定时器已初始化，当前应用未启用额外数据链路。
- FDCAN1：`FDCAN_FRAME_FD_BRS`，中断驱动（`IT0 + IT1`）。
- SPI1：主机模式 2 线 16 位，当前未接 DMA/中断数据链路。
- USB Device（CDC FS）：在 `defaultTask` 中启动（`MX_USB_Device_Init -> USBD_Start`），使用 USB LP/HP 中断 + PCD 回调。

## 当前硬件配置快照

- 平台：`STM32G431`，基于 CubeMX 生成，使用 HAL + FreeRTOS（CMSIS-RTOS v2）。
- 当前状态：已完成大部分外设初始化与 IRQ/DMA 连线，应用层收发与命令解析链路尚未接入。

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
  - 异步串口，`115200`，`8N1`，TX/RX 使能。
  - 路线：`UART中断 + RX DMA(循环) + TX DMA(普通)`。
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

## git提交备注

```text
feat(comm): 第一次提交，只将需要移植的通信架构文件拷贝进了项目，还为实现移植

[背景]
- 问题/需求: FOC通过精确控制电机的磁场方向，实现高效、稳定的运行。
- 触发条件: 现工程仅有外设初始化，缺少统一通信架构、异步收发与命令解析链路。

[改动]
- 新增目录:

- 修改文件:

- 关键点:


[影响评估]
- 硬件影响: 
- 实时性影响: 
- 资源影响: 
- 兼容性影响: 保留 CubeMX 框架

[验证]
- 构建: cmake --preset Debug && cmake --build --preset Debug -j 8
- 板测:
  - [ ] 
  - [ ] 
  - [ ] 
  - [ ] 

[回滚]
- 
- 重点回滚文件:
  - 
  - 
  - 
  - 
```

```
feat(comm): 实现灯语系统、MT6816移植与模拟EEPROM存储链路

[背景]
- 问题/需求: FOC通过精确控制电机的磁场方向，实现高效、稳定的运行。
- 触发条件: 现工程仅有外设初始化，缺少统一通信架构、异步收发与命令解析链路；同时缺少编码器接入、状态指示与参数持久化能力。

[改动]
- 新增目录:
  - Ctrl/Sensor/Encoder
  - Platform/Sensor/Encoder
  - Platform/Signal/LED
  - Platform/Memory
  - docs

- 修改文件:
  - CMakeLists.txt
  - UserApp/main.cpp
  - UserApp/freertos_inc.h
  - UserApp/common_inc.h
  - UserApp/protocols/ascii_protocol.cpp
  - Ctrl/Sensor/Encoder/mt6816_base.h
  - Ctrl/Sensor/Encoder/mt6816_base.cpp
  - Platform/Sensor/Encoder/mt6816_stm32.h
  - Platform/Sensor/Encoder/mt6816_stm32.cpp
  - Ctrl/Signal/LED/led_base.h
  - Ctrl/Signal/LED/led_base.cpp
  - Platform/Signal/LED/led_stm32.cpp
  - README.md

- 关键点:
  - 完成 MT6816 在当前工程目录结构下的移植，SPI读角度链路打通，支持校验位与无磁检测标志读取。
  - 新增 USB CDC 调试输出，周期打印 MT6816 关键数据（raw/rect/chk/nomag）。
  - 重构灯语系统：基于 Motor::RunState_t 输出三色灯语，所有灯语模式在 800ms 内完成一次完整序列。
  - 新增 peripheralTaskHandle，用于按状态机轮播演示灯语（STOP->RUNNING->FINISH->NO_CALIB->STALL->OVERLOAD）。
  - 接入模拟 EEPROM 方案（基于 Flash 随机读写工具）用于后续参数持久化扩展。
  - README 增补中文灯语说明与轮播规则，便于联调与交付。

[影响评估]
- 硬件影响:
  - 无新增外设；复用现有 SPI1、USB CDC 与板载三色LED引脚。
- 实时性影响:
  - 新增灯语与编码器打印任务为低优先级周期任务，对控制主链路影响可控。
- 资源影响:
  - 增加少量 Flash/RAM 占用（状态机、任务栈、日志字符串、编码器封装）。
- 兼容性影响:
  - 保留 CubeMX 框架；通信与外设初始化流程兼容现有工程。

[验证]
- 构建: cmake --preset Debug && cmake --build --preset Debug -j 8
- 板测:
  - [ ] USB CDC 可持续输出 MT6816 数据且无卡死
  - [ ] MT6816 角度更新正常，校验与 noMag 标志符合预期
  - [ ] 灯语按状态轮播，且每种模式 800ms 内完整执行
  - [ ] 上电/复位后模拟EEPROM读写行为符合预期

[回滚]
- git revert <this_commit_hash>
- 重点回滚文件:
  - UserApp/main.cpp
  - Ctrl/Sensor/Encoder/mt6816_base.cpp
  - Platform/Sensor/Encoder/mt6816_stm32.cpp
  - Ctrl/Signal/LED/led_base.cpp
```

- - 

