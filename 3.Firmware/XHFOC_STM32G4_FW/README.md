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



## 移植模块技术路线实施

### 通信架构（USART3 + USB CDC）

- 总体分层：`传输层(USART3/USB)` -> `协议层(ASCII/CMD)` -> `业务层(Motor/参数读写/状态查询)`。
- USART3 技术路线：`中断 + DMA` 组合。
  - 接收：`RX DMA 循环模式` 持续搬运数据，降低高频中断负载。
  - 发送：`TX DMA 普通模式`，按帧触发发送完成回调。
  - 同步：通过 `FreeRTOS 信号量` 与任务解耦，中断仅做事件投递。
- USB CDC 技术路线：`端点回调 + 接收队列 + 发送互斥`。
  - 接收回调写入软件缓冲并投递 `sem_usb_rx`。
  - 协议任务在任务上下文解析命令，避免在中断执行重逻辑。
  - 发送侧通过 `sem_usb_tx` 保证单次发送窗口，防止重入覆盖。
  - 端点规划：`EP1(CDC VCP)` + `EP3(Native/ODrive)` 双端点并行通信。

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

- 方式：使用 Flash 尾页作为“模拟 EEPROM”区域，提供字节级读写。
- 结构：
  - `Platform/Memory/random_flash_utils.*`：页擦写与缓冲刷写。
  - `Platform/Memory/random_flash_interface.*`：EEPROM 类接口封装。
- 路线：
  - 先读入 RAM 缓冲，再按需写回 Flash，降低擦写频次。
  - 为参数持久化（标定值、运行参数）预留统一接口。

## git提交备注

```text
feat(comm): XHFOC_STM32G4_FW第一次提交，cubemx配置代码提交，暂未接入运行链路和功能代码移植

[背景]
- 问题/需求: FOC通过精确控制电机的磁场方向，实现高效、稳定的运行。
- 触发条件: 现工程仅有外设初始化，缺少统一通信架构、异步收发与命令解析链路。

[改动]
- 新增目录:
  - 通信架构相关目录（协议层、传输层、平台适配层；本次以文件导入为主）
- 修改文件:
  - 无核心逻辑改动（仅拷贝移植文件并纳入工程）
- 关键点:
  - 完成通信架构代码入库，建立后续移植的目录与文件基础。
  - 暂未接入 USART3 / USB CDC 的异步收发流程。
  - 暂未接入命令分发、解析与执行链路。
  - 暂未改动现有 CubeMX 外设初始化流程。

[影响评估]
- 硬件影响: 无新增硬件连接变更。
- 实时性影响: 当前仅静态文件导入，对实时路径无新增负担。
- 资源影响: 增加少量 Flash 占用（代码文件导入），RAM运行态影响可忽略。
- 兼容性影响: 保留 CubeMX 框架

[验证]
- 构建: cmake --preset Debug && cmake --build --preset Debug -j 8
- 板测:
  - [ ] USART3 接收链路接入并可稳定收包
  - [ ] USART3 发送链路接入并可稳定发包
  - [ ] 命令解析链路可从输入到回包闭环
  - [ ] 异常帧/超时处理符合预期

[回滚]
- git revert <this_commit_hash>
- 重点回滚文件:
  - <通信架构新增目录1>
  - <通信架构新增目录2>
  - <通信架构核心文件1>
  - <通信架构核心文件2>
```





