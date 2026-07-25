# XHFOC DWT 与 RTOS 运行诊断硬件在环测试报告

- 测试日期：2026-07-26（UTC+8）
- 工作区：`E:\Projects\XHFOC`
- 固件：当前工作区未提交版本
- 构建类型：Release
- 目标芯片：STM32G43x/G44x，Cortex-M4，170 MHz
- 主要目标：
  1. 验证 DWT 对 `focMotor.Tick()` 最大执行时间和 ADC→FOC 任务唤醒延迟的测量。
  2. 验证 missed-tick、任务栈余量、CPU 运行时间与堆余量统计。
  3. 实机运行 FOC，并通过 921600 baud 串口采集和分析结果。
  4. 验证复位后复用 Flash 中的编码器对齐记录，不重复执行机械校准。

## 1. 结论摘要

本轮固件可以正常构建、下载、校验、复位、进入 FOC ready、响应 `!START`、连续运行 25 秒、响应 `!STOP`，并在后续复位时复用 Flash 校准记录。

| 验收项 | 结果 | 主要证据 |
| --- | --- | --- |
| Release 构建 | 通过 | ELF 成功生成，SHA-256 见下文 |
| ST-LINK 下载与校验 | 通过 | 芯片识别正确，`File download complete`、`Download verified successfully`、`Core run` |
| Flash 校准复用 | 通过 | `record=valid align=flash stored=1` |
| FOC ready | 通过 | `[foc] ready, waiting for !START` |
| FOC 启停 | 通过 | `Started ok`；非零 PWM 精确覆盖 5.000–30.000 s；STOP 后占空比为 0 |
| `Tick()` 最大执行时间小于 100 µs | 通过 | 实机累计最大 53.25 µs |
| 正常运行时 ADC→任务唤醒延迟小于 100 µs | 通过 | 首次统计前累计最大 17.04 µs |
| 启用全任务快照后 `missed == 0` | **不通过** | 每次 10 秒任务快照后稳定增加 2 个 missed-tick |
| 任务栈与 FreeRTOS 堆 | 本轮通过 | 最小任务栈余量 244 B；最小堆余量 6912 B |
| 921600 baud 连续采集 | 通过 | 35 秒无重连/无采集错误，估算 998.63 帧/s |
| VOFA/ASCII 共用串口的帧完整性 | 有已知缺陷 | STOP 响应插入二进制帧，产生 1 个有限值伪帧 |

总体判定：

- DWT 时序测量、missed-tick 计数和任务资源统计均能输出有效数据。
- FOC 控制本体满足 100 µs 周期内的执行时间要求，板卡可以稳定启停。
- 当前 `uxTaskGetSystemState()` 全任务快照不是无扰动测量：它会暂停调度约 0.25 ms，并在 10 kHz FOC 下每次造成 2 个 missed-tick。因此，这一版适合调试定位，不宜原样作为生产固件的常开统计实现。

## 2. 测试对象与当前控制条件

### 2.1 固件和控制参数

- 控制模式：`Motor::ANGLE`
- FOC 触发频率：10 kHz，周期 100 µs
- 本次未通过 USB 设置新目标；`!START` 锁存启动时实际位置
- 启动/目标位置：约 6.108695 rad
- 配置母线电压：12.0 V
- 电流限制：10 A
- 位置模式电流限制：2.5 A
- 速度限制：500 rad/s
- 位置加速度限制：30 rad/s²
- 角度 PID：P=3，I=0，D=0，输出限制 10 rad/s
- 速度 PID：P=0.22，I=0，D=0，输出斜坡 80，限制 12 A

注意：12.0 V 是固件配置值，本轮未独立测量实际母线电压。ST-LINK 报告的 3.22 V 是目标 MCU/SWD 电压，不是电机母线电压。电机型号、机械负载和环境温度未记录，因此不能把本轮电流与时序峰值直接外推到所有负载工况。

### 2.2 串口与帧格式

- 串口：COM8
- 参数：921600/8N1，无流控
- RX：128 B DMA 环形缓冲，Receive-to-IDLE/半满/满事件通知
- VOFA：JustFloat，16 通道，68 B/帧，约 1000 帧/s
- ASCII 命令和诊断信息与 VOFA 二进制帧共用同一串口

理论线上吞吐为 92,160 B/s（8N1 每字节按 10 bit 计算）。VOFA 主数据约为 68,000 B/s，占理论线速约 73.8%，尚余约 26.2% 给 ASCII、调度抖动和驱动开销。

## 3. 本轮实现说明

### 3.1 DWT 运行时间基准

FreeRTOS 调度器启动时使能 Cortex-M4 的 DWT `CYCCNT`。FreeRTOS run-time stats 使用一个软件累加器，将 170 MHz 原始周期按 256 分频：

- 调度切换热路径中不做整数除法。
- 保留低 8 bit 余数，避免短任务片段累计丢失。
- 用无符号差值处理 DWT 32 bit 原始计数器回绕。

### 3.2 ADC→任务唤醒延迟

测量边界如下：

1. 进入 `ADC1_2_IRQHandler` 时立即读取 `DWT->CYCCNT`。
2. ADC injected conversion complete 回调递增事件计数，并用计数型任务通知唤醒 `focControlTask`。
3. `focControlTask` 从 `ulTaskNotifyTake()` 返回后再次读取 DWT。
4. 两次读数之差更新累计 `wake_max`。

该数值包含 HAL ADC 中断处理、采样快照、ISR 通知、调度切换以及更高优先级/禁止调度区造成的等待，不只是 FreeRTOS notify 指令本身。

### 3.3 `focMotor.Tick()` 执行时间

FOC 任务在调用 `focMotor.Tick()` 前后读取 DWT，以差值更新累计 `tick_max`。当前输出的是“本次上电、时序统计使能以来的累计最大值”，不会在每个 10 秒窗口自动清零。

### 3.4 missed-tick

ADC 事件和任务已处理事件分别使用单调递增计数器。FOC 任务一次唤醒时：

- 只有 1 个待处理事件：执行 1 次 `Tick()`，不增加 missed。
- 有 N 个待处理事件：仍只执行 1 次 `Tick()`，其余 N-1 次累加到 `missed`。

这种定义可以直接发现任务没有跟上 10 kHz 触发，但它不补算过期控制周期，避免在恢复后连续执行多个使用过期采样的控制 tick。

### 3.5 任务运行时间、栈和堆

`peripheralTask` 每 10 秒输出一次：

- `[rtos]`：uptime、FOC tick、missed、`tick_max`、`wake_max`、当前/历史最小堆余量。
- `[rtos.task]`：各任务上一统计窗口 CPU 占比、优先级、状态和任务创建以来的最小剩余栈。

任务快照容量为 10，本轮活动任务数为 8。快照期间临时从 FreeRTOS 堆申请缓冲，输出结束后释放。

## 4. 构建与烧录

### 4.1 构建

执行：

```powershell
cmake --preset Release
cmake --build --preset Release -j 8
```

结果：

- Release 配置和构建成功。
- ELF：`3.Firmware/XHFOC_STM32G4_FW/build/Release/XHFOC_STM32G4_FW.elf`
- ELF SHA-256：`5D41B360809CF05902B8095AFDFE0DB75ED1A1E350B4E586EBB3DC75AE3E5702`
- GNU size：text 93,356 B，data 1,056 B，bss 30,840 B
- 链接器 Flash 区占用：94,424 B / 129,024 B，73.18%
- 链接器 RAM 区占用：31,896 B / 32,768 B，97.34%

RAM 链接占用已接近上限，只剩约 872 B 静态地址空间。FreeRTOS 堆数组已经计入该 RAM 占用，因此运行时堆还有余量并不等于还可以继续增加大量全局/静态数据。

### 4.2 下载、校验与复位

使用 STM32CubeProgrammer CLI 2.22.0，通过 ST-LINK V2、SWD 4 MHz、Normal 模式和软件复位完成：

- 目标电压：3.22 V
- Device ID：0x468
- 目标：STM32G43x/G44x
- NVM：128 KB
- ELF 下载完成
- 下载内容校验成功
- 软件复位成功
- Cortex-M4 Core run

报告未记录或公开 ST-LINK 序列号。

## 5. 采集会话

### 5.1 FOC 运行会话

目录：

`4.Software/FOC-Serial-Analyzer/captures/xhfoc_20260726_024841_670254`

流程：

- 采集时长：35 s
- 5.000 s：发送 `!START`
- 30.000 s：发送 `!STOP`
- 35.000 s：收尾再次发送 `!STOP`
- 采集结束后关闭串口，并通过 ST-LINK 执行 `-rst -run`

元数据：

- `status=complete`
- `stop_reason=duration_elapsed`
- 原始字节：2,379,968 B
- 操作系统读取块：73,988
- 连接记录：1 次
- 重连：0 次
- 错误：0
- 原始文件大小与 metadata 一致

解码：

- 解码帧：34,953
- 找到帧尾：34,954
- 解码器拒绝帧尾：1
- NaN/Inf 帧：0
- 非帧字节：3,164
- ASCII 片段：41
- 估算帧率：998.63 帧/s
- 每 1 秒帧数范围：989–1001

### 5.2 复位启动观察会话

目录：

`4.Software/FOC-Serial-Analyzer/captures/xhfoc_20260726_025513_608579`

流程：

- 先打开 COM8
- 约 2 s 时通过 ST-LINK 软件复位
- 不发送 `!START`
- 总采集 15 s

元数据：

- `status=complete`
- 原始字节：879,590 B
- 连接记录：1 次
- 重连：0 次
- 错误：0
- 原始文件大小与 metadata 一致

复位后的关键时间点：

| 采集时间 | 日志/事件 |
| ---: | --- |
| 约 2.110 s | 系统启动日志开始 |
| 约 4.110 s | 电流零点测量完成 |
| 约 4.110 s | `record=valid align=flash stored=1` |
| 约 4.110 s | `[foc] ready, waiting for !START` |
| 约 14.110 s | 首个 RTOS 10 秒诊断窗口 |

`record=valid align=flash` 是本轮对“后续复位不再重新校准”的直接证据。约 2 秒初始化时间主要包含启动和电流零点测量；没有观察到重新执行编码器方向/电角度机械对齐。

## 6. 实时性结果

### 6.1 首次统计前的基线

启动观察会话的首个统计点：

```text
[rtos] uptime_ms=12004 foc_ticks=99973 missed=0
       tick_max=5254 cyc (30.90 us)
       wake_max=2898 cyc (17.04 us)
```

这组数据在第一次调用全任务快照之前读取，因此最能代表未受任务统计快照干扰的停止待机状态：

- 约 10 秒内执行 99,973 个 FOC tick，接近 10 kHz。
- missed=0。
- `Tick()` 最大 30.90 µs。
- ADC IRQ 入口到 FOC 任务恢复运行最大 17.04 µs。

### 6.2 FOC 运行时

运行会话中的累计最大值：

| 采集时间 | FOC 状态 | `tick_max` | `wake_max` | missed |
| ---: | --- | ---: | ---: | ---: |
| 2.750 s | START 前 | 32.04 µs | 254.80 µs | 10 |
| 12.750 s | 已运行约 7.75 s | 50.42 µs | 254.80 µs | 12 |
| 22.765 s | 完整运行窗口 | 53.25 µs | 254.80 µs | 14 |
| 32.765 s | STOP 后约 2.75 s | 53.25 µs | 254.80 µs | 16 |

`Tick()` 的实机最大值 53.25 µs，占 100 µs 控制周期的 53.25%，单看控制计算仍有约 46.75 µs 余量。

不能把累计 `tick_max` 和累计 `wake_max` 简单相加当成同一次事件的端到端最坏值，因为两者可能来自不同 tick。本轮更有意义的判断是：

- 首次统计前：wake 17.04 µs、missed=0，调度正常。
- 执行任务快照后：wake 累计峰值升到 254.80 µs，并出现 missed。

### 6.3 统计功能的观测扰动

运行会话中 missed 从 10、12、14 增至 16，每个 10 秒统计周期稳定增加 2。启动观察会话的第一次 `[rtos]` 在调用任务快照前先读取计数，因此打印 `missed=0`；该快照造成的丢 tick 会在下一个周期才显示。

源码中的 `uxTaskGetSystemState()` 会调用 `vTaskSuspendAll()`，并在调度器暂停期间遍历所有任务、扫描各任务栈高水位。ADC 中断仍能进入并累计通知，但最高优先级 FOC 任务不能被调度。实测 254.80 µs 的最大唤醒延迟足以跨过约 2 个 100 µs 控制周期，与每次增加 2 个 missed 完全吻合。

因此可以作出以下分级结论：

- **已验证**：每个统计周期后增加 2 个 missed；最大唤醒延迟达到 254.80 µs。
- **有源码和时序强证据支持的原因判断**：`uxTaskGetSystemState()` 暂停调度是主要来源。
- **尚未直接用 GPIO/逻辑分析仪确认**：调度暂停的精确起止点和每一步耗时。

## 7. CPU、任务栈和堆

完整 FOC 运行窗口（约 12.765–22.765 s）的 CPU 分布：

| 任务 | 优先级 | 配置栈 | 本轮最小剩余栈 | 峰值已用栈 | CPU |
| --- | ---: | ---: | ---: | ---: | ---: |
| `focControlTask` | 48 | 2048 B | 744 B* | 1304 B | 47.2% |
| `IDLE` | 0 | 512 B | 428 B | 84 B | 48.5% |
| `vofaTask` | 16 | 1024 B | 404 B | 620 B | 3.9% |
| `peripheralTask` | 24 | 1024 B | 244 B | 780 B | 0.3% |
| `UartServerTask` | 32 | 1536 B | 656 B | 880 B | 0.0% |
| `UsbServerTask` | 32 | 1536 B | 860 B | 676 B | 0.0–0.1% |
| `usbIrqTask` | 32 | 500 B | 364 B | 136 B | 0.0% |
| `Tmr Svc` | 2 | 1024 B | 892 B | 132 B | 0.0% |

\* `focControlTask` 在完整运行会话中记录为 776 B；另一独立复位会话的最小值为 744 B，因此表中采用两次会话的更保守值。

说明：

- 完整运行窗口中 `focControlTask + vofaTask + peripheralTask` 约占 51.4%，IDLE 约 48.5%，CPU 总体仍有余量。
- 最紧张的任务栈是 `peripheralTask`，余量 244 B，约为配置栈的 23.8%。本轮未溢出，但后续继续向诊断函数增加大型局部变量或格式化逻辑时风险最高。
- `UartServerTask` 在处理命令后最小余量下降到 656 B，仍有约 42.7%。
- 当前 FreeRTOS 堆余量为 8024 B，历史最小余量为 6912 B；活动任务 8，快照容量 10。
- 没有出现 `task_snapshot=overflow`、`task_snapshot=no_heap` 或 `task_snapshot=failed`。

## 8. FOC 与 VOFA 数据分析

为了避免命令切换瞬间影响，稳定运行统计窗口取 6.0–29.5 s，共 23,479 帧，帧率约 999.11 Hz。

| 指标 | 结果 |
| --- | ---: |
| 目标位置 | 6.108695 rad |
| 位置范围 | 6.108695–6.108695 rad |
| 绝对位置误差 mean/P95/max | 0 / 0 / 0 rad |
| 速度范围 | 0–0 rad/s |
| `|iq|` 峰值 | 0.02938 A |
| `|id|` 峰值 | 0.01748 A |
| `|iA|` 峰值 | 0.10381 A |
| `|iB|` 峰值 | 0.06988 A |
| `|iC|` 峰值 | 0.05993 A |
| dutyA 范围 | 0.499805–0.500065 |
| dutyB 范围 | 0.500031–0.500195 |
| dutyC 范围 | 0.499896–0.500078 |

本轮没有位置阶跃，`!START` 只保持当前角度，因此“误差为零、速度为零”表示编码器分辨率内没有观察到运动，不能代替带负载位置跟踪或动态阶跃测试。

启停证据：

- START 前 1.0–4.5 s：三相 duty 和电流输出均为 0。
- 非零 duty 首次出现在 5.000 s，最后出现在 30.000 s，与命令时间一致。
- 运行窗口内三相 duty 在 0.5 附近微调，电流很小，符合无位置误差的保持状态。
- STOP 后 30.5–34.5 s：三相 duty 全部为 0。
- `Started ok` 被解码为 ASCII。
- `Stopped ok\r\n` 可在原始字节约 offset 2,040,145 处直接看到，但它插入了一帧二进制数据中，因此没有作为完整 ASCII 行出现在 `ascii.log`。

## 9. 串口混合流异常

解码器报告 1 个被拒绝帧尾；此外还发现 1 个“所有 float 都是有限值、但明显超出物理范围”的伪帧：

- 时间：30.000 s，恰好为 STOP 响应时刻
- `position=4095`
- `velocity=2042`
- `target=2148`
- `adcRawIa≈2.96e29`

原始字节显示 `Stopped ok\r\n` 被插入 VOFA 帧中间。JustFloat 只有固定帧尾、没有 CRC；解码器围绕后续帧尾重同步时，这段混合字节碰巧被解释成有限浮点数。

按照项目技能规定的物理范围过滤规则，该帧应剔除。剔除后有效物理帧为 34,952。本报告的电流、速度和位置统计均未使用该伪帧。

这不是 921600 baud 本身造成的随机丢字节：会话没有重连或系统串口错误，伪帧与 ASCII 响应插入位置精确重合。根因是 ASCII 和二进制发送在协议层没有保证整帧原子性。

## 10. 验证清单

已执行并通过：

- `cmake --preset Release`
- `cmake --build --preset Release -j 8`
- STM32CubeProgrammer 下载 ELF
- 下载后逐字节 verify
- 软件复位与 Core run
- COM8 921600/8N1 35 秒 FOC 运行采集
- 定时 `!START`、`!STOP` 和收尾 `!STOP`
- 采集后 ST-LINK 安全复位
- 独立复位启动观察采集
- 两个会话均先确认 `metadata.json status=complete`
- 两个会话均运行 `decode_capture.py`
- 串口分析器单元测试：10/10 通过
- `git diff --check` 通过，仅有工作区 LF→CRLF 提示

未执行：

- 带机械负载的动态阶跃测试
- 示波器/逻辑分析仪 GPIO 时序复核
- 长时间热机或压力测试
- 多板一致性测试
- Debug 构建性能测试

## 11. 涉及的源码和文档

本轮时序诊断实现涉及：

- `3.Firmware/XHFOC_STM32G4_FW/Core/Inc/FreeRTOSConfig.h`
- `3.Firmware/XHFOC_STM32G4_FW/Core/Src/app_freertos.c`
- `3.Firmware/XHFOC_STM32G4_FW/Core/Src/stm32g4xx_it.c`
- `3.Firmware/XHFOC_STM32G4_FW/Core/Src/adc.c`
- `3.Firmware/XHFOC_STM32G4_FW/UserApp/main.cpp`
- `3.Firmware/XHFOC_STM32G4_FW/README.md`
- `5.Docs/xhfoc-hil-debug/SKILL.md`

工作区还存在用户已有或无关改动；本轮没有提交、重置、删除或推送任何文件。采集数据保留在分析器的 `captures` 目录中，未加入 Git。

## 12. 风险与建议

### 12.1 优先修复：消除任务快照造成的 missed-tick

建议不要在 10 kHz FOC 正常运行时直接调用一次性 `uxTaskGetSystemState()`：

1. 将完整任务快照改为显式调试命令触发，生产默认关闭；或
2. 对已知任务句柄逐个调用轻量查询，把 8 个任务分散到多个低优先级周期中，允许 FOC 在查询之间抢占；或
3. 使用 FreeRTOS trace hook 在任务切换时维护运行时间，栈高水位改为低频、分任务扫描；或
4. 只在电机 STOP 状态执行全量栈扫描，运行时仅输出 ISR/FOC 的无锁累计计数。

修复后的硬件验收条件应为：

- 连续运行至少 60 秒；
- 至少跨过 6 个诊断周期；
- `missed` 始终为 0；
- `wake_max < 100 µs`；
- `tick_max < 100 µs`；
- CPU 和各任务栈数据仍可稳定输出。

### 12.2 改善时序统计可解释性

- 增加“本窗口最大值”和“上电累计最大值”两组指标，报告后清零窗口值。
- 增加 ADC 事件序号或 MCU 时间戳，避免把操作系统串口读取时间误当成精确采样时间。
- 若需要分析长尾，增加分桶直方图或 P99/P99.9，而不只记录最大值。

### 12.3 修复 ASCII/VOFA 帧交织

- 让单个 VOFA 帧作为不可拆分的发送事务进入 UART TX 队列。
- ASCII 行只能在两个完整 VOFA 帧之间插入。
- 更稳妥的长期方案是为二进制遥测增加长度、序号和 CRC，或将日志与遥测拆分到不同接口。
- 解码器应把位置/目标、速度、电流和 ADC 原始值的物理范围过滤纳入正式候选帧校验，而不是仅拒绝 NaN/Inf。

### 12.4 RAM 与任务容量

- 链接 RAM 已达 97.34%，继续增加静态缓冲前必须重新检查 map。
- `peripheralTask` 只有 244 B 最小栈余量，应避免继续在该任务调用链中增加大局部数组和重格式化逻辑。
- 任务快照容量 10、当前任务数 8，只剩 2 个任务余量；增加任务时必须同步检查容量，但不建议在当前 RAM 状态下直接扩大静态结构。

## 13. 最终判断

本次修改达成了“能够量化 FOC 执行时间、唤醒延迟、missed-tick、CPU、任务栈和堆”的目标，并成功在真实板卡上暴露出一个仅靠静态审查难以确认的问题：全任务统计快照本身会破坏 10 kHz 控制任务的实时性。

因此建议保留 DWT 和 missed-tick 基础测量，下一步优先重构任务快照机制，再做一次不少于 60 秒的硬件复测。当前版本可以用于调试分析，但不应把“每 10 秒全量任务快照”视为已满足零丢 tick 的最终生产实现。
