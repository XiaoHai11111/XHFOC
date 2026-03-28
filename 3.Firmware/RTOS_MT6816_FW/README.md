# RTOS_MT6816_FW 

## 灯语说明（FOC 状态指示）

### 状态与灯语映射

| 电机状态 | 红灯 | 黄灯 | 绿灯 | 说明 |
|---|---|---|---|---|
| `STATE_STOP` | 灭 | 单短闪（800ms 周期） | 灭 | 系统空闲/待机 |
| `STATE_RUNNING` | 灭 | 灭 | 心跳双闪（800ms 周期） | 闭环运行中 |
| `STATE_FINISH` | 灭 | 灭 | 常亮 | 动作完成并保持 |
| `STATE_NO_CALIB` | 灭 | 单闪突发（800ms 周期） | 灭 | 编码器未标定或标定数据无效 |
| `STATE_STALL` | 双闪突发（800ms 周期） | 灭 | 灭 | 堵转告警 |
| `STATE_OVERLOAD` | 三闪突发（800ms 周期） | 灭 | 灭 | 过载告警 |

### 代码位置

- 灯语逻辑：`Ctrl/Signal/LED/led_base.cpp`
- STM32 灯脚映射：`Platform/Signal/LED/led_stm32.cpp`
- 演示任务：`UserApp/main.cpp` 中的 `ThreadPeripheral` / `peripheralTaskHandle`

### 轮播说明

- `peripheralTaskHandle` 每 `50ms` 调用一次 `statusLed.Tick(...)`。
- 每 `5s` 按状态机顺序切换：`STOP -> RUNNING -> FINISH -> NO_CALIB -> STALL -> OVERLOAD -> STOP`。
- 所有灯语模式均在 `800ms` 内完成一次完整序列并循环。
- 每次状态切换会在 USB CDC 输出：
  - `[led] simulate state A -> B`

## 通信框架移植

- 将3rdParty、Platform、UserApp文件夹拷贝进当前目录

## MT6816移植



## LED移植



## KEY按键移植



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

