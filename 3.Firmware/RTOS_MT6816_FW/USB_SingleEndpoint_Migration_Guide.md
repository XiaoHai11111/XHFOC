# USB 单端点移植流程（对比 `First commit: fcd61d5`）

## 1. 文档目的
本文档用于记录从仓库第一次提交 `fcd61d5 (First commit)` 到当前版本（`V1.0.1 + 当前工作区修复`）的 USB 单端点通信移植过程，重点说明：

- 为什么改
- 改了哪些文件
- 每个文件“修改前/修改后”的关键代码
- 最终如何验证（含压力测试）

## 2. 对比范围与结论

### 2.1 基线与目标
- 基线版本：`fcd61d5 (First commit)`
- 目标版本：`14d80ce (V1.0.1)` + 当前工作区 `usbd_cdc.c` 稳定性补丁

### 2.2 全量差异（基线 -> 当前）
- `26 files changed, 545 insertions(+), 197 deletions(-)`（包含工程与通信相关改动）
- USB 单端点移植核心文件见第 4 节

### 2.3 最终状态
- USB 单端点收发链路稳定
- 首次/二次发送无回包问题已定位并修复
- 已完成压力测试（连续收发场景通过）

---

## 3. 目标链路（单端点）

主链路如下：

1. Host 发送 ASCII 命令到 CDC OUT EP1
2. `USBD_CDC_DataOut -> CDC_Receive_FS -> usb_rx_process_packet`
3. `UsbServerTask` 拉起处理，进入 `ASCII_protocol_parse_stream`
4. `OnUsbAsciiCmd` 生成响应
5. `USBSender::process_packet -> CDC_Transmit_FS (强制 CDC EP1) -> IN 完成回调`
6. `CDC_TransmitCplt_FS` 释放 `sem_usb_tx`，允许下一包发送

---

## 4. 逐文件移植步骤（含修改前/后）

## Step A: 通道与任务基础（RTOS + 通信线程）

### A1. `Core/Src/app_freertos.c`
目的：增加 USB/UART 通信同步原语，保证通信任务在 RTOS 中可运行。

修改前：
```c
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
```

修改后：
```c
/* USER CODE BEGIN Variables */
osSemaphoreId sem_usb_irq;
osSemaphoreId sem_uart3_dma;
osSemaphoreId sem_usb_rx;
osSemaphoreId sem_usb_tx;
```

并新增初始化：
```c
osSemaphoreDef(sem_usb_irq);
sem_usb_irq = osSemaphoreNew(1, 0, osSemaphore(sem_usb_irq));

osSemaphoreDef(sem_uart3_dma);
sem_uart3_dma = osSemaphoreNew(1, 1, osSemaphore(sem_uart3_dma));

osSemaphoreDef(sem_usb_rx);
sem_usb_rx = osSemaphoreNew(1, 0, osSemaphore(sem_usb_rx));

osSemaphoreDef(sem_usb_tx);
sem_usb_tx = osSemaphoreNew(1, 1, osSemaphore(sem_usb_tx));
```

并在默认任务中接入主逻辑：
```c
Main();
```

---

### A2. `UserApp/freertos_inc.h`
目的：头文件对齐 UART3 DMA 信号量命名。

修改前：
```c
extern osSemaphoreId sem_uart1_dma;
```

修改后：
```c
extern osSemaphoreId sem_uart3_dma;
```

---

### A3. `Platform/Communication/communication.cpp`
目的：USB IRQ 延后处理 + printf 输出路由修正到 UART3。

修改前：
```cpp
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
uart1StreamOutputPtr->process_bytes(...);
```

修改后：
```cpp
extern PCD_HandleTypeDef hpcd_USB_FS;
HAL_PCD_IRQHandler(&hpcd_USB_FS);
HAL_NVIC_EnableIRQ(USB_LP_IRQn);
uart3StreamOutputPtr->process_bytes(...);
```

---

## Step B: UART1 -> UART3 统一（避免链路错配）

### B1. `3rdParty/fibre/include/protocol.hpp`
修改前：
```cpp
CHANNEL_TYPE_USB,
CHANNEL_TYPE_UART1,
CHANNEL_TYPE_UART4,
```

修改后：
```cpp
CHANNEL_TYPE_USB,
CHANNEL_TYPE_UART1,
CHANNEL_TYPE_UART3,
CHANNEL_TYPE_UART4,
```

---

### B2. `Platform/Communication/interface_uart.hpp`
修改前：
```cpp
extern StreamSink *uart1StreamOutputPtr;
```

修改后：
```cpp
extern StreamSink *uart3StreamOutputPtr;
```

---

### B3. `Platform/Communication/interface_uart.cpp`
目标：发送器、DMA、串口实例、回调全部统一到 USART3。

修改前（示例）：
```cpp
class UART1Sender ...
channelType = CHANNEL_TYPE_UART1;
osSemaphoreAcquire(sem_uart1_dma, ...);
HAL_UART_Transmit_DMA(&huart1, ...);
if (huart->Instance == USART1) osSemaphoreRelease(sem_uart1_dma);
```

修改后（示例）：
```cpp
class UART3Sender ...
channelType = CHANNEL_TYPE_UART3;
osSemaphoreAcquire(sem_uart3_dma, ...);
HAL_UART_Transmit_DMA(&huart3, ...);
if (huart->Instance == USART3) osSemaphoreRelease(sem_uart3_dma);
```

---

### B4. `UserApp/main.cpp`
修改前：
```cpp
Respond(*uart1StreamOutputPtr, ...);
```

修改后：
```cpp
Respond(*uart3StreamOutputPtr, ...);
```

---

## Step C: ASCII 解析按通道隔离（解决 USB/UART 串扰）

### C1. `Platform/Communication/ascii_processor.hpp`
修改前：
```cpp
void OnUart1AsciiCmd(...);
```

修改后：
```cpp
void OnUart3AsciiCmd(...);
```

---

### C2. `Platform/Communication/ascii_processor.cpp`
目的：解析状态从“全局单份”改为“每个通道独立一份”。

修改前：
```cpp
static uint8_t parse_buffer[MAX_LINE_LENGTH];
static bool read_active = true;
static uint32_t parse_buffer_idx = 0;
```

修改后：
```cpp
struct AsciiParseState {
    uint8_t parse_buffer[MAX_LINE_LENGTH];
    bool initialized;
    bool read_active;
    uint32_t parse_buffer_idx;
};
static AsciiParseState parse_states[StreamSink::CHANNEL_TYPE_UART5 + 1] = {};
```

收益：USB 和 UART 同时收包时互不污染解析状态，减少偶发“命令收到但无响应”。

---

### C3. `UserApp/protocols/ascii_protocol.cpp`
修改前：
```cpp
void OnUart1AsciiCmd(...)
```

修改后：
```cpp
void OnUart3AsciiCmd(...)
```

并新增无效命令兜底：
```cpp
else {
    Respond(_responseChannel, "Invalid command");
}
```

---

## Step D: USB 单端点发送/接收接口收敛

### D1. `USB_Device/App/usbd_cdc_if.h`
目的：为上层发送提供统一常量和带端点参数的接口。

修改前：
```c
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);
```

修改后：
```c
#define USB_RX_DATA_SIZE APP_RX_DATA_SIZE
#define USB_TX_DATA_SIZE APP_TX_DATA_SIZE
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len, uint8_t endpoint_pair);
```

---

### D2. `USB_Device/App/usbd_cdc_if.c`
目标：发送回调与发送入口按单端点行为收敛。

关键修改 1：接收回调从“立即重装包”改为投递到通信线程。

修改前：
```c
USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
USBD_CDC_ReceivePacket(&hUsbDeviceFS);
```

修改后：
```c
usb_rx_process_packet(Buf, *Len, endpoint_pair);
```

关键修改 2：发送完成回调仅在 CDC IN EP 完成时释放令牌。

修改后：
```c
if ((epnum & 0x7FU) != (CDC_IN_EP & 0x7FU))
  return USBD_OK;
hcdc->CDC_Tx.State = 0U;
osSemaphoreRelease(sem_usb_tx);
```

关键修改 3：`CDC_Transmit_FS` 强制单端点（CDC EP1）发送。

修改后：
```c
(void) endpoint_pair; // force CDC single endpoint
if (hcdc->CDC_Tx.State != 0) return USBD_BUSY;
memcpy(CDCTxBufferFS, Buf, Len);
USBD_CDC_SetTxBuffer(&hUsbDeviceFS, CDCTxBufferFS, Len, CDC_OUT_EP);
USBD_CDC_TransmitPacket(&hUsbDeviceFS, CDC_OUT_EP);
```

---

### D3. `Platform/Communication/interface_usb.cpp`
目标：上层发送从“超时丢包”改为“阻塞 + 可恢复重试”，并修复分包 bug。

修改前：
```cpp
if (osSemaphoreAcquire(sem_usb_tx_, PROTOCOL_SERVER_TIMEOUT_MS) != osOK) {
    usb_stats_.tx_overrun_cnt++;
}
status = CDC_Transmit_FS(buffer, length, endpoint_pair_);
if (output_.process_packet(buffer, length) != 0) ...
```

修改后：
```cpp
osSemaphoreAcquire(sem_usb_tx_, osWaitForever);
for (size_t retry = 0; retry < 100; ++retry) {
    status = CDC_Transmit_FS(buffer, length, endpoint_pair_);
    if (status == USBD_OK) break;
    if (status != USBD_BUSY) break;
    osDelay(1);
}
if (output_.process_packet(buffer, chunk) != 0) ...
```

收益：
- 降低瞬时 `USBD_BUSY` 导致的回复丢失
- 修复流式发送按整段长度调用导致的分包异常

---

## Step E: CDC 中间件结构改造与首包稳定性修复

### E1. `Middlewares/.../Class/CDC/Inc/usbd_cdc.h`
目标：支持端点对结构化状态（即使当前只启用单端点，也保留兼容）。

修改前：
```c
int8_t (* Receive)(uint8_t *Buf, uint32_t *Len);
uint8_t USBD_CDC_SetTxBuffer(..., uint32_t length);
uint8_t USBD_CDC_TransmitPacket(USBD_HandleTypeDef *pdev);
```

修改后：
```c
int8_t (* Receive)(uint8_t *, uint32_t *, uint8_t);
uint8_t USBD_CDC_SetTxBuffer(..., uint16_t length, uint8_t endpoint_pair);
uint8_t USBD_CDC_TransmitPacket(USBD_HandleTypeDef *pdev, uint8_t endpoint_pair);
```

并新增 `USBD_CDC_EP_HandleTypeDef` 与 `CDC_Tx/CDC_Rx/...` 字段。

---

### E2. `Middlewares/.../Class/CDC/Src/usbd_cdc.c`
这是“首次/二次发送无回包”最终修复点。

#### 修复点 1：初始化阶段对齐实际端点状态
修改前：
```c
hcdc->TxState = 0U;
hcdc->RxState = 0U;
USBD_LL_PrepareReceive(..., hcdc->RxBuffer, ...);
```

修改后：
```c
hcdc->TxState = 0U;
hcdc->RxState = 0U;
hcdc->CDC_Tx.State = 0U;
hcdc->CDC_Rx.State = 0U;
hcdc->REF_Tx.State = 0U;
hcdc->ODRIVE_Rx.State = 0U;
USBD_LL_PrepareReceive(..., hcdc->CDC_Rx.Buffer, ...);
```

#### 修复点 2：发送元数据保持一致（DataIn/ZLP 路径）
在 `USBD_CDC_SetTxBuffer` 中补齐：
```c
if (endpoint_pair == CDC_OUT_EP) {
    hcdc->TxBuffer = pbuff;
    hcdc->TxLength = length;
}
```

在 `USBD_CDC_TransmitPacket` 中补齐：
```c
pdev->ep_in[in_ep & 0xFU].total_length = hEP_Tx->Length;
hcdc->TxBuffer = hEP_Tx->Buffer;
hcdc->TxLength = hEP_Tx->Length;
```

收益：修复“刚打开 USB 前几次发包无返回”的核心一致性问题。

---

## Step F: CubeMX 配置同步

### F1. `RTOS_MT6816_FW.ioc`
关键同步项：
- `FREERTOS.configTOTAL_HEAP_SIZE=10240`
- 中断优先级微调：`USART3_IRQn` 从 `5` 调整为 `6`
- `USB_HP_IRQn` 使能修正

这些配置用于保障通信任务与 USB 中断在当前工程中的运行一致性。

---

## 5. 验证流程（已执行）

## 5.1 编译验证
```bash
cmake --preset Debug
cmake --build --preset Debug -j 8
```
结果：通过。

## 5.2 功能验证
- USB 枚举：通过
- 单端点 ASCII 收发：通过
- 上电后首次/二次发送回包：通过（已修复）
- 长时间压力测试：通过（无稳定复现丢包）

建议压测口径（可复现执行）：
- 命令：`!START` / `!STOP` 交替
- 频率：5~20ms/条
- 时长：5~10 分钟
- 判据：发送条数 == 收到应答条数，且无卡死

---

## 6. 回滚策略

若需回滚 USB 单端点移植，优先回滚以下文件：

1. `Platform/Communication/interface_usb.cpp`
2. `USB_Device/App/usbd_cdc_if.c`
3. `Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c`
4. `Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc/usbd_cdc.h`

命令示例：
```bash
git revert <commit_hash>
```

---

## 7. 备注
- 本文档以 `fcd61d5` 为对比基线。
- 当前仓库若继续向“多端点/复合设备”演进，建议在保持本单端点稳定基线的前提下增量启用 EP3，不要与单端点修复混改。
