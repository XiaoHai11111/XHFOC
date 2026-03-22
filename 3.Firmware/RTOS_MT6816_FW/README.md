# RTOS_MT6816_FW 

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
feat(comm): 迁移 USB_Uart_Cmd 通信架构到 G431 RTOS 工程（USB CDC + UART3 DMA Circular）

[背景]
- 问题/需求: 需要在 RTOS_MT6816_FW 中复用 USB_Uart_Cmd 的 USB/UART 通信分层与命令通道。
- 触发条件: 现工程仅有外设初始化，缺少统一通信架构、异步收发与命令解析链路。

[改动]
- 新增目录:
  - Platform/Communication/*
  - UserApp/*
  - 3rdParty/fibre/{protocol.cpp, include/protocol.hpp, include/crc.hpp, include/cpp_utils.hpp}
- 修改文件:
  - CMakeLists.txt
  - Core/Inc/FreeRTOSConfig.h
  - Core/Src/main.c
  - Core/Src/app_freertos.c
  - Core/Src/usart.c
  - USB_Device/App/usbd_cdc_if.c
  - USB_Device/App/usbd_cdc_if.h
- 关键点:
  - 适配为单 CDC + USART3（替换 USB_Uart_Cmd 中 F4 双 CDC / USART1 依赖）
  - UART3 RX DMA 改为 Circular，通信线程轮询解析
  - USB RX 改为线程侧处理，USB TX 完成回调释放信号量
  - 默认任务接入 UserApp::Main() 启动通信框架

[影响评估]
- 硬件影响: 使用现有 USB FS + USART3，不新增外设。
- 实时性影响: 新增通信线程与信号量，同步路径从中断改为线程处理。
- 资源影响: 增加 C++ 通信模块与任务栈，RAM/Flash 上升。
- 兼容性影响: 保留 CubeMX 框架；用户协议通道新增但不改底层存储布局。

[验证]
- 构建: cmake --preset Debug && cmake --build --preset Debug -j 8
- 板测:
  - [ ] USB CDC 收发正常
  - [ ] UART3 DMA Circular 接收与命令解析正常
  - [ ] printf 同步输出到 USB + UART
  - [ ] 基础 ASCII 命令响应正常

[回滚]
- 回退本次新增目录与接入改动文件到上一提交。
- 重点回滚文件:
  - CMakeLists.txt
  - Core/Src/app_freertos.c
  - Core/Src/usart.c
  - USB_Device/App/usbd_cdc_if.c
```

